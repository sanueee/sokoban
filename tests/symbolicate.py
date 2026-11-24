#!/usr/bin/env python3
"""
Символизация профилей samply (Firefox Profiler .json / .json.gz) для MinGW GCC
на Windows через DWARF (addr2line.exe) + печать сводной таблицы CPU для отчёта.

Использование:
    python symbolicate.py <profile.json.gz> <sokoban_bench.exe> [output.json.gz]

Пример (из sokoban\build):
    python ..\tests\symbolicate.py ..\tests\profiling-Windows\v1\windows_profile_v1.json.gz .\sokoban_bench.exe
"""

import argparse
import gzip
import json
import os
import re
import shutil
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path


def find_mingw_tool(name: str) -> str:
    found = shutil.which(name)
    if found:
        return found
    for candidate in [
        rf"C:\mingw64\bin\{name}.exe",
        rf"C:\msys64\mingw64\bin\{name}.exe",
        rf"C:\msys64\ucrt64\bin\{name}.exe",
    ]:
        if os.path.isfile(candidate):
            return candidate
    return name


def detect_image_base(exe_path: str, objdump_bin: str) -> int:
    try:
        res = subprocess.run(
            [objdump_bin, "-p", exe_path],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=True,
        )
        m = re.search(r"ImageBase\s+([0-9a-fA-F]+)", res.stdout)
        if m:
            return int(m.group(1), 16)
    except Exception:
        pass
    return 0x140000000


def load_profile(path: Path) -> dict:
    with open(path, "rb") as f:
        magic = f.read(2)
    if magic == b"\x1f\x8b":
        with gzip.open(path, "rt", encoding="utf-8") as gf:
            return json.load(gf)
    with open(path, "rt", encoding="utf-8") as tf:
        return json.load(tf)


def resolve_addresses(exe_path: str, addr2line_bin: str, image_base: int, addrs: list[int]) -> dict:
    if not addrs:
        return {}
    hex_addrs = [hex(image_base + a) for a in addrs]
    res = subprocess.run(
        [addr2line_bin, "-e", exe_path, "-f", "-i", "-C", "-a"],
        input="\n".join(hex_addrs) + "\n",
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=True,
    )

    addr_map = {}
    cur_addr = None
    lines = res.stdout.splitlines()
    idx = 0
    while idx < len(lines):
        line = lines[idx].strip()
        if line.startswith("0x") or line.startswith("0X"):
            cur_addr = int(line, 16) - image_base
            addr_map[cur_addr] = []
            idx += 1
        else:
            func = line
            loc = lines[idx + 1].strip() if idx + 1 < len(lines) else "??:0"
            idx += 2
            if ":" in loc:
                fpart, lpart = loc.rsplit(":", 1)
                tok = lpart.split()[0] if lpart.split() else ""
                lnum = int(tok) if tok.isdigit() else None
            else:
                fpart, lnum = loc, None

            fpart_norm = fpart.replace("\\", "/")
            if "sokoban/" in fpart_norm:
                fpart = "sokoban/" + fpart_norm.rsplit("sokoban/", 1)[1]
            elif fpart == "??":
                fpart = None

            if cur_addr is not None:
                addr_map[cur_addr].append((func, fpart, lnum))

    # Разворачиваем порядок: 0 = внешняя функция, -1 = самая внутренняя inline-функция
    for a in addr_map:
        addr_map[a].reverse()
    return addr_map


def get_string_array(thread: dict) -> list:
    if "stringArray" in thread:
        return thread["stringArray"]
    return thread["stringTable"]


def symbolicate_profile(prof: dict, exe_path: str, addr2line_bin: str, objdump_bin: str) -> tuple[dict, int]:
    exe_name = os.path.basename(exe_path).lower()
    target_lib_indices = {
        i
        for i, lib in enumerate(prof.get("libs", []))
        if exe_name in str(lib.get("name", "")).lower()
        or exe_name in str(lib.get("path", "")).lower()
    }
    if not target_lib_indices:
        raise RuntimeError(f"Библиотека '{exe_name}' не найдена в списке libs профиля.")

    image_base = detect_image_base(exe_path, objdump_bin)

    unique_addrs = set()
    for t in prof.get("threads", []):
        rt = t["resourceTable"]
        ft = t["frameTable"]
        fn_t = t["funcTable"]
        for fr_i in range(ft["length"]):
            fn_i = ft["func"][fr_i]
            res_i = fn_t["resource"][fn_i]
            if res_i is not None and res_i >= 0 and rt["lib"][res_i] in target_lib_indices:
                addr = ft["address"][fr_i]
                if addr is not None and addr >= 0:
                    unique_addrs.add(addr)

    addr_map = resolve_addresses(exe_path, addr2line_bin, image_base, sorted(unique_addrs))

    for t in prof.get("threads", []):
        rt = t["resourceTable"]
        ft = t["frameTable"]
        fn_t = t["funcTable"]
        st_t = t["stackTable"]
        sa = get_string_array(t)

        str_idx = {s: i for i, s in enumerate(sa)}

        def intern(s):
            if s is None:
                return None
            if s not in str_idx:
                str_idx[s] = len(sa)
                sa.append(s)
            return str_idx[s]

        func_cache = {}

        def get_or_add_func(fname, res_i, fpath):
            key = (fname, res_i, fpath)
            if key in func_cache:
                return func_cache[key]
            idx = fn_t["length"]
            for col, vals in fn_t.items():
                if col == "length":
                    continue
                if col == "name":
                    vals.append(intern(fname))
                elif col in ("isJS", "relevantForJS"):
                    vals.append(False)
                elif col == "resource":
                    vals.append(res_i)
                elif col == "fileName":
                    vals.append(intern(fpath) if fpath else None)
                else:
                    vals.append(None)
            fn_t["length"] += 1
            func_cache[key] = idx
            return idx

        orig_frame_len = ft["length"]
        frame_expansion = {}

        for fr_i in range(orig_frame_len):
            fn_i = ft["func"][fr_i]
            res_i = fn_t["resource"][fn_i]
            if res_i is not None and res_i >= 0 and rt["lib"][res_i] in target_lib_indices:
                addr = ft["address"][fr_i]
                chain = [c for c in addr_map.get(addr, []) if c[0] != "??"]
                if not chain:
                    continue

                outer_name, outer_file, outer_line = chain[0]
                ft["func"][fr_i] = get_or_add_func(outer_name, res_i, outer_file)
                if "line" in ft:
                    ft["line"][fr_i] = outer_line
                if "inlineDepth" in ft:
                    ft["inlineDepth"][fr_i] = 0

                f_ids = [fr_i]
                for depth, (iname, ifile, iline) in enumerate(chain[1:], 1):
                    new_fr = ft["length"]
                    ifn = get_or_add_func(iname, res_i, ifile)
                    for col, vals in ft.items():
                        if col == "length":
                            continue
                        if col == "func":
                            vals.append(ifn)
                        elif col == "inlineDepth":
                            vals.append(depth)
                        elif col == "line":
                            vals.append(iline)
                        elif col == "column":
                            vals.append(None)
                        else:
                            vals.append(vals[fr_i])
                    ft["length"] += 1
                    f_ids.append(new_fr)

                if len(f_ids) > 1:
                    frame_expansion[fr_i] = f_ids

        if frame_expansion:
            new_stack_map = {}
            stack_cache = {}
            list_cols = [k for k in st_t.keys() if k != "length"]
            new_cols = {k: [] for k in list_cols}

            def add_stack(pref, fr, template_old_i):
                key = (pref, fr)
                if key in stack_cache:
                    return stack_cache[key]
                idx = len(new_cols["frame"])
                for col in list_cols:
                    if col == "prefix":
                        new_cols[col].append(pref)
                    elif col == "frame":
                        new_cols[col].append(fr)
                    else:
                        new_cols[col].append(st_t[col][template_old_i])
                stack_cache[key] = idx
                return idx

            def convert_stack(old_i):
                if old_i is None:
                    return None
                if old_i in new_stack_map:
                    return new_stack_map[old_i]
                pref = convert_stack(st_t["prefix"][old_i])
                fr = st_t["frame"][old_i]
                f_ids = frame_expansion.get(fr, [fr])
                cur = pref
                for f_id in f_ids:
                    cur = add_stack(cur, f_id, old_i)
                new_stack_map[old_i] = cur
                return cur

            t["samples"]["stack"] = [convert_stack(s) for s in t["samples"]["stack"]]
            for col in list_cols:
                st_t[col] = new_cols[col]
            st_t["length"] = len(new_cols["frame"])

    prof.setdefault("meta", {})["symbolicated"] = True
    return prof, len(unique_addrs)


def print_cpu_summary(prof: dict, top_n: int = 20):
    # Берём поток с наибольшим числом сэмплов (главный поток)
    threads = prof.get("threads", [])
    if not threads:
        return
    t0 = max(threads, key=lambda t: len(t.get("samples", {}).get("stack", [])))
    stacks = t0["samples"]["stack"]
    st_t = t0["stackTable"]
    ft = t0["frameTable"]
    fn_t = t0["funcTable"]
    sa = get_string_array(t0)

    total = sum(1 for s in stacks if s is not None)
    if total == 0:
        print("Нет сэмплов в профиле.")
        return

    self_c = Counter()
    incl_c = Counter()
    inlined = set()
    file_of = {}

    for s in stacks:
        if s is None:
            continue
        chain = []
        cur = s
        while cur is not None:
            fr_i = st_t["frame"][cur]
            fn_i = ft["func"][fr_i]
            fname = sa[fn_t["name"][fn_i]]
            if "inlineDepth" in ft and ft["inlineDepth"][fr_i] and ft["inlineDepth"][fr_i] > 0:
                inlined.add(fname)
            fidx = fn_t.get("fileName", [None] * fn_t["length"])[fn_i]
            if fidx is not None and fname not in file_of:
                file_of[fname] = sa[fidx]
            chain.append(fname)
            cur = st_t["prefix"][cur]

        self_c[chain[0]] += 1
        for fname in set(chain):
            incl_c[fname] += 1

    print(f"\nПоток: {t0.get('name')} | Всего сэмплов: {total}")
    print(f"{'Функция':<32} {'Inclusive':>16} {'Self':>16}  Файл")
    print("-" * 90)
    shown = 0
    for fname, cnt in incl_c.most_common():
        fp = file_of.get(fname, "")
        if not fp and fname not in ("main", "SolveLevel", "GenerateLevel"):
            continue
        sc = self_c[fname]
        label = f"{fname} (inlined)" if fname in inlined else fname
        print(
            f"{label:<32} {cnt*100/total:5.1f}% ({cnt:6d})   {sc*100/total:5.1f}% ({sc:6d})  {fp}"
        )
        shown += 1
        if shown >= top_n:
            break


def main():
    parser = argparse.ArgumentParser(
        description="Символизация профиля samply через DWARF (MinGW addr2line)."
    )
    parser.add_argument("profile", help="Путь к .json.gz или .json профилю от samply")
    parser.add_argument("exe", help="Путь к sokoban_bench.exe (собранному с -gdwarf-4)")
    parser.add_argument(
        "output",
        nargs="?",
        help="Выходной .json.gz (по умолчанию <name>_symbolicated.json.gz)",
    )
    parser.add_argument("--addr2line", default=None, help="Путь к addr2line.exe")
    parser.add_argument("--objdump", default=None, help="Путь к objdump.exe")
    parser.add_argument("--top", type=int, default=20, help="Сколько функций вывести в таблицу")
    args = parser.parse_args()

    prof_path = Path(args.profile)
    exe_path = str(Path(args.exe).resolve())
    if args.output:
        out_path = Path(args.output)
    else:
        stem = prof_path.name
        for suf in (".json.gz", ".json"):
            if stem.endswith(suf):
                stem = stem[: -len(suf)]
                break
        out_path = prof_path.with_name(f"{stem}_symbolicated.json.gz")

    addr2line_bin = args.addr2line or find_mingw_tool("addr2line")
    objdump_bin = args.objdump or find_mingw_tool("objdump")

    prof = load_profile(prof_path)
    prof, n_addrs = symbolicate_profile(prof, exe_path, addr2line_bin, objdump_bin)

    with gzip.open(out_path, "wt", encoding="utf-8") as gf:
        json.dump(prof, gf)

    print(f"Готово: расшифровано {n_addrs} адресов -> {out_path}")
    print_cpu_summary(prof, top_n=args.top)


if __name__ == "__main__":
    main()