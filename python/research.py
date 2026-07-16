"""Read binary bar logs from the C++ pipeline and export research artifacts.

Why: a tiny Python reader shows the bar log is research-ready without pulling the
hot path into a heavier analytics stack or notebook runtime.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

BAR_STRUCT = struct.Struct("<IQQIIIIQB7x")
BAR_SIZE = BAR_STRUCT.size


def read_bars(path: Path) -> list[dict]:
    data = path.read_bytes()
    if len(data) % BAR_SIZE != 0:
        raise ValueError(f"truncated bar log: {len(data)} bytes")
    bars = []
    for off in range(0, len(data), BAR_SIZE):
        fields = BAR_STRUCT.unpack_from(data, off)
        bars.append(
            {
                "symbol_id": fields[0],
                "start_ts_ns": fields[1],
                "end_ts_ns": fields[2],
                "open": fields[3],
                "high": fields[4],
                "low": fields[5],
                "close": fields[6],
                "volume": fields[7],
                "bar_type": fields[8],
            }
        )
    return bars


def export_parquet(bars: list[dict], out: Path) -> None:
    try:
        import pyarrow as pa
        import pyarrow.parquet as pq
    except ImportError as exc:
        raise SystemExit("pyarrow is required for parquet export") from exc
    table = pa.Table.from_pylist(bars)
    pq.write_table(table, out)


def plot_session(bars: list[dict], out: Path) -> None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise SystemExit("matplotlib is required for plotting") from exc

    time_bars = [b for b in bars if b["bar_type"] == 0]
    spreads = []
    rates = []
    for b in time_bars:
        spreads.append(b["high"] - b["low"])
        dt = max(b["end_ts_ns"] - b["start_ts_ns"], 1)
        rates.append(b["volume"] / (dt / 1e9))

    fig, axes = plt.subplots(1, 2, figsize=(10, 4))
    axes[0].hist(spreads, bins=30, color="#2f5d50")
    axes[0].set_title("Bar range distribution")
    axes[0].set_xlabel("high-low (ticks)")
    axes[1].plot(rates, color="#1f3a5f")
    axes[1].set_title("Implied message rate proxy")
    axes[1].set_xlabel("time bar index")
    axes[1].set_ylabel("volume / second")
    fig.tight_layout()
    fig.savefig(out)
    plt.close(fig)


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="Research helpers for binary bar logs")
    p.add_argument("bars", type=Path, help="path to bars.bin")
    p.add_argument("--parquet", type=Path, help="optional parquet output path")
    p.add_argument("--plot", type=Path, help="optional plot png output path")
    args = p.parse_args(argv)

    bars = read_bars(args.bars)
    print(f"bars={len(bars)}")
    if args.parquet:
        export_parquet(bars, args.parquet)
        print(f"wrote {args.parquet}")
    if args.plot:
        plot_session(bars, args.plot)
        print(f"wrote {args.plot}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
