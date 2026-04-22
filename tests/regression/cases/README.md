# Regression Cases

Each maintained regression case has its own directory under `tests/regression/cases/`.

Layout:
- Provide `compute.dl` and `input/*.facts` plus optional `input/*.prob`.

The runner (`tests/regression/run_regression_case.py`) copies these static
assets into a per-case build directory before compiling with the repo-built
`souffle` binary.
