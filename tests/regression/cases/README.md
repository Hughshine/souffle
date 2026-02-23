# Regression Cases

Each maintained regression case has its own directory under `tests/regression/cases/`.

Layout:
- Static case: provide `compute.dl` and `input/*.facts` (plus optional `input/*.prob`).
- Dynamic case: provide a per-case `generate.py` that writes `compute.dl` and `input/*`
  into the output directory passed via `--out-dir`.

The runner (`tests/regression/run_regression_case.py`) copies static assets first, then
invokes `generate.py` when present.
