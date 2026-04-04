import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np

df = pd.read_csv(
    "tests_res.csv",
    sep=";",
    dtype={"difficulty": str, "num_boxes": int,
           "gen_ms": float, "solve_ms": float, "solved": int},
)

DIFFICULTY_ORDER = ["easy", "medium", "hard"]
DIFFICULTY_RU    = {"easy": "Лёгкий", "medium": "Средний", "hard": "Тяжёлый"}
COLORS           = {"easy": "#4CAF50", "medium": "#FF9800", "hard": "#F44336"}

print("  СТАТИСТИКА ПО СЛОЖНОСТИ (мс)")

for metric, label in [("gen_ms", "Генерация"), ("solve_ms", "Решение")]:
    print(f"\n{'─'*65}")
    print(f"  {label}")
    print(f"{'─'*65}")
    print(f"  {'Сложность':<12} {'Min':>10} {'Mean':>10} {'Max':>10}  {'N':>6}")
    print(f"  {'─'*52}")
    for diff in DIFFICULTY_ORDER:
        sub = df[df.difficulty == diff][metric]
        if sub.empty:
            continue
        print(f"  {DIFFICULTY_RU[diff]:<12} "
              f"{sub.min():>10.2f} "
              f"{sub.mean():>10.2f} "
              f"{sub.max():>10.2f}  "
              f"{len(sub):>6}")

print(f"\n{'='*65}\n")

boxes_stat = (
    df.groupby(["difficulty", "num_boxes"])[["gen_ms", "solve_ms"]]
      .mean()
      .reset_index()
)

fig, axes = plt.subplots(1, 2, figsize=(14, 6))

for ax, metric, title in [
    (axes[0], "gen_ms",   "Генерация уровня"),
    (axes[1], "solve_ms", "Решение (BFS)"),
]:
    for diff in DIFFICULTY_ORDER:
        sub = boxes_stat[boxes_stat.difficulty == diff].sort_values("num_boxes")
        if sub.empty:
            continue
        ax.plot(
            sub["num_boxes"], sub[metric],
            marker="o", linewidth=2.2, markersize=7,
            color=COLORS[diff], label=DIFFICULTY_RU[diff],
        )
        # подписи значений
        for _, row in sub.iterrows():
            ax.annotate(
                f"{row[metric]:.0f}",
                xy=(row["num_boxes"], row[metric]),
                xytext=(4, 5), textcoords="offset points",
                fontsize=8, color=COLORS[diff],
            )

    ax.set_title(title, fontsize=13, pad=8)
    ax.set_xlabel("Количество ящиков", fontsize=11)
    ax.set_ylabel("Среднее время, мс", fontsize=11)
    ax.set_yscale("log")                      # лог-шкала из-за разброса
    ax.yaxis.set_major_formatter(
        ticker.FuncFormatter(lambda v, _: f"{v:,.0f}")
    )
    ax.set_xticks(sorted(df["num_boxes"].unique()))
    ax.legend(fontsize=10)
    ax.grid(True, which="both", linestyle="--", alpha=0.45)
    ax.spines[["top", "right"]].set_visible(False)

plt.tight_layout()
plt.show()