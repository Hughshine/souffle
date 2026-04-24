# Regression Cases

Each maintained case is incremental-artifact focused and runs through
`tests/regression/run_regression_case.py`.

Layout:

- static case: `compute.dl` plus `input/*.facts` and optional `input/*.prob`
- dynamic case: `generate.py` writes `compute.dl` and `input/*` into the
  workspace passed via `--out-dir`

Do not add non-incremental artifact or benchmark-pipeline fixtures to this
branch's regression suite.
