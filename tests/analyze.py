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

def print_boxes_table(title, data, col_label):
    print(f"\nТаблица: {title}")
    print(f"{'Кол-во ящиков':<16} {'Среднее (мс)':>14} {'Мин (мс)':>12} {'Макс (мс)':>12}")
    print(f"{'─'*56}")
    for _, row in data.iterrows():
        print(f"{int(row['num_boxes']):<16} {row['mean']:>14.2f} {row['min']:>12.2f} {row['max']:>12.2f}")


# Таблица 1: Генерация
gen_stat = (
    df.groupby("num_boxes")["gen_ms"]
      .agg(mean="mean", min="min", max="max")
      .reset_index()
)
print_boxes_table("Статистика времени генерации (мс)", gen_stat, "gen_ms")

# Таблица 2: Решение
solve_stat = (
    df.groupby("num_boxes")["solve_ms"]
      .agg(mean="mean", min="min", max="max")
      .reset_index()
)
print_boxes_table("Статистика времени решения (мс)", solve_stat, "solve_ms")

# Таблица 3: Успех решения
print(f"\nТаблица: Успех поиска решений")
print(f"{'Кол-во ящиков':<16} {'Решено':>8} {'Всего':>8} {'% успеха':>10}")
print(f"{'─'*44}")
success_stat = df.groupby("num_boxes")["solved"].agg(solved="sum", total="count").reset_index()
for _, row in success_stat.iterrows():
    pct = row["solved"] / row["total"] * 100
    print(f"{int(row['num_boxes']):<16} {int(row['solved']):>8} {int(row['total']):>8} {pct:>9.1f}%")

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
plt.savefig("plot.png", dpi=150, bbox_inches="tight")
plt.show()