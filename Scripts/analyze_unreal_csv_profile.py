#!/usr/bin/env python3
"""Summarize an Unreal CSV profiler capture without third-party packages."""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path


def percentile(values: list[float], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] * (upper - position) + ordered[upper] * (position - lower)


def summarize(values: list[float]) -> dict[str, float | int | None]:
    if not values:
        return {"count": 0}
    mean = sum(values) / len(values)
    return {
        "count": len(values),
        "mean": mean,
        "median": percentile(values, 0.5),
        "p90": percentile(values, 0.9),
        "p95": percentile(values, 0.95),
        "p99": percentile(values, 0.99),
        "p999": percentile(values, 0.999),
        "min": min(values),
        "max": max(values),
    }


def parse_capture(path: Path) -> tuple[list[str], list[list[float]], dict[str, str]]:
    with path.open("r", newline="", encoding="utf-8-sig") as stream:
        rows = csv.reader(stream)
        header = next(rows)
        numeric_rows: list[list[float]] = []
        metadata: dict[str, str] = {}
        for row in rows:
            if not row:
                continue
            if row[0] == "EVENTS":
                continue
            if row[0] == "[HasHeaderRowAtEnd]":
                for index in range(2, len(row) - 1, 2):
                    key = row[index].strip("[]").lower()
                    metadata[key] = row[index + 1]
                continue
            values: list[float] = []
            for value in row:
                try:
                    values.append(float(value) if value else 0.0)
                except ValueError:
                    values.append(0.0)
            if len(values) < len(header):
                values.extend([0.0] * (len(header) - len(values)))
            numeric_rows.append(values[: len(header)])
    return header, numeric_rows, metadata


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path", type=Path)
    parser.add_argument("--warmup-frames", type=int, default=300)
    args = parser.parse_args()

    header, rows, metadata = parse_capture(args.csv_path)
    columns: dict[str, int] = {}
    for index, name in enumerate(header):
        columns.setdefault(name, index)

    wanted = [
        "FrameTime",
        "GameThreadTime",
        "RenderThreadTime",
        "GPUTime",
        "RHIThreadTime",
        "MaxFrameTime",
        "RHI/DrawCalls",
        "RHI/PrimitivesDrawn",
        "GPUSceneInstanceCount",
        "GPUMem/LocalUsedMB",
        "GPUMem/LocalBudgetMB",
        "RenderTargetPoolUsed",
        "RenderTargetPool/PeakUsedMB",
        "TransientMemoryUsedMB",
        "PhysicalUsedMB",
        "VirtualUsedMB",
        "MemoryFreeMB",
        "TextureStreaming/DesiredDataLoadedPercent",
        "TextureStreaming/StreamingPool",
        "TextureStreaming/WantedMips",
        "TextureStreaming/NonStreamingMips",
        "NaniteStreaming/RootDataSizeMB",
        "DistanceField/AtlasMB",
        "RayTracingGeometry/TotalResidentSizeMB",
        "RDGCount/Passes",
        "RDGCount/Buffers",
        "RDGCount/Textures",
        "LightCount/All",
        "ActorCount/TotalActorCount",
        "View/PosX",
        "View/PosY",
        "View/PosZ",
        "View/Speed",
        "View/Speed2D",
        "PSO/PSOMisses",
        "PSO/PSOComputeMisses",
    ]

    def window_summary(window_rows: list[list[float]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for name in wanted:
            if name not in columns:
                continue
            values = [row[columns[name]] for row in window_rows]
            result[name] = summarize(values)

        frame_times = [row[columns["FrameTime"]] for row in window_rows]
        frame_stats = summarize(frame_times)
        p99 = frame_stats.get("p99")
        p999 = frame_stats.get("p999")
        result["derived"] = {
            "average_fps": 1000.0 / frame_stats["mean"] if frame_stats.get("mean") else None,
            "one_percent_low_fps": 1000.0 / p99 if p99 else None,
            "point_one_percent_low_fps": 1000.0 / p999 if p999 else None,
            "frames_over_16_67ms": sum(value > 16.67 for value in frame_times),
            "frames_over_33_33ms": sum(value > 33.33 for value in frame_times),
            "frames_over_50ms": sum(value > 50.0 for value in frame_times),
        }

        thread_names = ["GameThreadTime", "RenderThreadTime", "GPUTime"]
        available_threads = [name for name in thread_names if name in columns]
        bottleneck_counts = {name: 0 for name in available_threads}
        for row in window_rows:
            bottleneck = max(available_threads, key=lambda name: row[columns[name]])
            bottleneck_counts[bottleneck] += 1
        result["bottleneck_frame_counts"] = bottleneck_counts
        return result

    warmup_count = min(args.warmup_frames, len(rows))
    output = {
        "source": str(args.csv_path.resolve()),
        "metadata": metadata,
        "frames": len(rows),
        "warmup_frames": warmup_count,
        "full_capture": window_summary(rows),
        "warmup": window_summary(rows[:warmup_count]),
        "steady_state": window_summary(rows[warmup_count:]),
    }
    print(json.dumps(output, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
