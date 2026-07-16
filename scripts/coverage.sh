#!/usr/bin/env bash
# Why: per-object gcov avoids header reports from one TU overwriting another
# and produces a single merged line rate for include/mdp and src.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
PART=$(mktemp -d)
trap 'rm -rf "$PART"' EXIT
idx=0
for dir in "$BUILD/CMakeFiles/mdp_core.dir/src" "$BUILD/tests/CMakeFiles/mdp_tests.dir"; do
  [[ -d "$dir" ]] || continue
  for o in "$dir"/*.o; do
    [[ -f "$o" ]] || continue
    base=$(basename "$o" .o)
    mkdir -p "$PART/$idx"
    cp "$dir/$base.gcno" "$PART/$idx/" 2>/dev/null || true
    cp "$dir/$base.gcda" "$PART/$idx/" 2>/dev/null || true
    cp "$o" "$PART/$idx/"
    (cd "$PART/$idx" && xcrun gcov -p -o . "$(basename "$o")" >/dev/null 2>&1 || true)
    idx=$((idx + 1))
  done
done
python3 - "$ROOT" "$PART" <<'PY'
import os, re, collections, sys
root, parts = sys.argv[1], sys.argv[2]
want = (root + "/src/", root + "/include/mdp/")
hits = collections.defaultdict(int)
exec_lines = collections.defaultdict(set)
for dp, _, fn in os.walk(parts):
  for name in fn:
    if not name.endswith(".gcov"):
      continue
    lines = open(os.path.join(dp, name)).readlines()
    if not lines:
      continue
    m = re.search(r"Source:(.*)", lines[0])
    if not m:
      continue
    src = m.group(1).strip()
    if not any(src.startswith(p) for p in want):
      continue
    for line in lines[1:]:
      mm = re.match(r"\s*([0-9]+|-|#####):\s*([0-9]+):", line)
      if not mm:
        continue
      tag, ln = mm.group(1), int(mm.group(2))
      if tag == "-":
        continue
      exec_lines[src].add(ln)
      if tag != "#####":
        hits[(src, ln)] = max(hits[(src, ln)], int(tag) if tag.isdigit() else 1)
total = hit = 0
print("=== Deduped project line coverage ===")
for src in sorted(exec_lines):
  t = len(exec_lines[src])
  h = sum(1 for ln in exec_lines[src] if hits[(src, ln)] > 0)
  total += t
  hit += h
  print(f"{100*h/t:6.2f}%  {h:4d}/{t:<4d}  {os.path.relpath(src, root)}")
print("------------------------------------")
print(f"TOTAL  {100*hit/max(total,1):.2f}%  {hit}/{total}")
PY
