# Test Case Context

## Case Asset Model

`tests/regression/cases/<case-id>/`:
- Static-first:
  - `compute.dl`
  - `input/*.facts`
  - optional `input/*.prob`
- Optional dynamic entry:
  - `generate.py` (invoked with `--out-dir <work-case-dir>`)

Runner behavior (`tests/regression/run_regression_case.py`):
1. Copy static assets from case directory into work dir.
2. Run `generate.py` when present.
3. Compile and execute according to case function.

## New Case Checklist

1. Add case assets under `tests/regression/cases/<case-id>/`.
2. Implement case function in `tests/regression/run_regression_case.py`.
3. Add `<case-id>` to `CASES` map.
4. Register CTest case in `tests/regression/CMakeLists.txt` with timeout.
5. Update docs case list if inventory changed.
6. Run regression suite.

## Timeout Heuristic

- Small full-only smoke/equivalence: 120-180s
- Incremental multi-turn comparisons: 180-240s
- Heavier graph/rederive dynamic generation: 240s

Use smaller values first; increase only if CI flakes.

## Dynamic Generator Template

```python
#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()

    out = Path(args.out_dir)
    (out / "input").mkdir(parents=True, exist_ok=True)
    (out / "compute.dl").write_text("...", encoding="utf-8")
    (out / "input" / "R.facts").write_text("...\\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```
