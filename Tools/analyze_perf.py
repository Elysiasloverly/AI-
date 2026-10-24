"""
一局性能数据自动分析（读 CSV Profiler 输出 + 运行日志）。

用法（在项目根目录执行）：
    python Tools/analyze_perf.py
    python Tools/analyze_perf.py --csv-dir "F:/AI_WorkSpeace/baka/Saved/Profiling/CSV" --logs "F:/AI_WorkSpeace/baka/Saved/Logs/perf_server.log" "F:/AI_WorkSpeace/baka/Saved/Logs/perf_client.log"

它会输出：
  1. CSV 里有哪些列（第一次跑时用来确认采集到了什么）
  2. 帧时间统计：平均 / P50 / P95 / P99 / 最差 10 帧（含行号对应的绝对时间）
  3. 各 CPU 分类的均值（GameThread / RenderThread / Net / Physics 等，取决于实际列名）
  4. 日志体检：fatal / ensure / warning 计数 + 关键行（GameplayCue 注册、归属校正、RPC 相关告警）
"""
from __future__ import annotations

import argparse
import glob
import os
import re
import statistics
import sys
from collections import Counter, defaultdict

DEFAULT_CSV_DIR = r"F:\AI_WorkSpeace\baka\Saved\Profiling\CSV"
DEFAULT_LOGS = [
    r"F:\AI_WorkSpeace\baka\Saved\Logs\perf_server.log",
    r"F:\AI_WorkSpeace\baka\Saved\Logs\perf_client.log",
]

# 帧时间列名在不同引擎版本里叫法略有差异，按优先级找
FRAME_TIME_CANDIDATES = ["FrameTime", "FrameTime_GameThread", "FrameTime_Total", "FrameTimeMS"]


def pick_frame_time_column(columns: list[str]) -> str | None:
    for name in FRAME_TIME_CANDIDATES:
        for column in columns:
            if column.strip().lower() == name.lower():
                return column
    # 退一步：任何带 frame 且带 time 的列
    for column in columns:
        lowered = column.lower()
        if "frame" in lowered and "time" in lowered:
            return column
    return None


def read_csv_rows(path: str) -> tuple[list[str], list[dict[str, str]]]:
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        lines = [line.rstrip("\n") for line in handle if line.strip()]
    if not lines:
        return [], []

    # CSV Profiler 的文件头几行是元信息（如 "EVENT,..." / 版本），找到第一行像表头的
    header_index = 0
    for index, line in enumerate(lines[:8]):
        if "," in line and not line.startswith("EVENT"):
            header_index = index
            break

    header = [column.strip() for column in lines[header_index].split(",")]
    rows: list[dict[str, str]] = []
    for line in lines[header_index + 1:]:
        if line.startswith("EVENT"):
            continue
        parts = line.split(",")
        if len(parts) < 2:
            continue
        rows.append({header[i]: parts[i] for i in range(min(len(header), len(parts)))})
    return header, rows


def to_float(value: str) -> float | None:
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def analyze_csv(csv_dir: str) -> None:
    files = sorted(glob.glob(os.path.join(csv_dir, "*.csv")))
    if not files:
        print(f"[CSV] 目录里没有 CSV：{csv_dir}")
        print("      确认采集时执行过 CsvProfiler Start / Stop（或 csvprofile start / stop）")
        return

    print(f"[CSV] 发现 {len(files)} 个文件：")
    for path in files:
        print(f"      - {os.path.basename(path)}  ({os.path.getsize(path) / 1024:.1f} KB)")

    for path in files:
        print(f"\n===== {os.path.basename(path)} =====")
        header, rows = read_csv_rows(path)
        print(f"列（{len(header)}）：{', '.join(header[:40])}{' ...' if len(header) > 40 else ''}")
        if not rows:
            print("没有数据行")
            continue

        frame_column = pick_frame_time_column(header)
        if frame_column is None:
            print("没找到帧时间列，跳过帧统计（把上面的列名发我，我按实际列名调整）")
            continue

        samples = [(index, to_float(row.get(frame_column, ""))) for index, row in enumerate(rows)]
        samples = [(index, value) for index, value in samples if value is not None and value > 0]
        if not samples:
            print(f"列 {frame_column} 里没有有效数值")
            continue

        values = [value for _, value in samples]
        values_sorted = sorted(values)
        count = len(values_sorted)

        def percentile(p: float) -> float:
            if count == 0:
                return 0.0
            position = min(count - 1, max(0, int(round(p / 100.0 * (count - 1)))))
            return values_sorted[position]

        print(f"采样帧数：{count}   列：{frame_column}")
        print(f"帧时间(ms)：平均 {statistics.fmean(values):.2f}   P50 {percentile(50):.2f}   "
              f"P95 {percentile(95):.2f}   P99 {percentile(99):.2f}   最差 {values_sorted[-1]:.2f}")
        print(f"对应 FPS：平均 {1000.0 / statistics.fmean(values):.1f}   P95 下限 {1000.0 / max(percentile(95), 0.001):.1f}")

        worst = sorted(samples, key=lambda item: item[1], reverse=True)[:10]
        print("最差 10 帧（行号, ms）：" + ", ".join(f"#{index}:{value:.1f}" for index, value in worst))

        # 找 CPU 分类列并给均值，方便看钱花在哪
        category_columns = [
            column for column in header
            if column != frame_column and re.search(r"GameThread|RenderThread|Net|Physics|Tick|AI|Animation", column, re.I)
        ]
        if category_columns:
            print("分类均值(ms)：")
            for column in category_columns[:14]:
                column_values = [value for value in (to_float(row.get(column, "")) for row in rows) if value is not None]
                if column_values:
                    print(f"      {column:<28} {statistics.fmean(column_values):7.2f}")


def analyze_logs(log_paths: list[str]) -> None:
    for path in log_paths:
        print(f"\n===== 日志 {os.path.basename(path)} =====")
        if not os.path.isfile(path):
            print("      文件不存在（确认启动参数里的 -abslog 路径）")
            continue

        counters: Counter[str] = Counter()
        key_lines: dict[str, list[str]] = defaultdict(list)
        interesting = {
            "fatal": re.compile(r"Fatal error|Critical error", re.I),
            "ensure": re.compile(r"Ensure condition failed", re.I),
            "error": re.compile(r"Error:", re.I),
            "cue注册": re.compile(r"原生 GameplayCue 注册"),
            "归属校正": re.compile(r"归属校正"),
            "网络告警": re.compile(r"LogNet: Warning|NetDriver|Serializ", re.I),
        }

        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                for name, pattern in interesting.items():
                    if pattern.search(line):
                        counters[name] += 1
                        if len(key_lines[name]) < 5:
                            key_lines[name].append(line.strip()[:220])

        for name in interesting:
            print(f"      {name}: {counters.get(name, 0)}")
        for name, lines in key_lines.items():
            if name in ("fatal", "ensure", "归属校正", "cue注册") and lines:
                print(f"      --- {name} 样例 ---")
                for line in lines:
                    print(f"        {line}")


def main() -> int:
    parser = argparse.ArgumentParser(description="分析一局联机性能数据")
    parser.add_argument("--csv-dir", default=DEFAULT_CSV_DIR)
    parser.add_argument("--logs", nargs="*", default=DEFAULT_LOGS)
    args = parser.parse_args()

    analyze_csv(args.csv_dir)
    analyze_logs(args.logs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
