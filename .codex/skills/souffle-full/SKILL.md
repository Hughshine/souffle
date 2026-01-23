---
name: souffle-full
description: Full-only Soufflé/ProbLog benchmark workflow for side_channel_full.py. Use when running full-mode experiments, rewrite vs no-rewrite comparisons, or collecting full benchmark results.
---

# Souffle Full

## Workflow (script-driven)
1. Build Soufflé and set PATH to the repo build:
   - `cmake -S . -B build`
   - `cmake --build build -j${JOBS}`
   - `PATH=/home/hugh/research/datalog/souffle/build/src:$PATH`
2. Generate inputs (SMT → Soufflé/ProbLog):
   - `python3 problog-benchmark/side_channel_full.py --base-dir <base> generate --cases 1-20 --cleanup --rule-set full`
3. Compile full-only binaries:
   - `python3 problog-benchmark/side_channel_full.py --base-dir <base> compile --cases 1-20 --timeout 300 --souffle-arg=<extra>`
   - The script always calls `souffle --online --full-only -F input -D output`.
4. Run full evaluations:
   - `python3 problog-benchmark/side_channel_full.py --base-dir <base> run --cases 1-20 --timeout 600 --souffle-arg=--det-opt`
   - Use `--problog-only` or `--souffle-only` to limit runs.
5. Collect results:
   - `python3 problog-benchmark/side_channel_full.py --base-dir <base> collect --cases 1-20`

## Notes
- Use a separate `--base-dir` when comparing rewrite vs no-rewrite so outputs do not overwrite each other.
- Extra runtime flags pass via `--souffle-arg` (repeatable), e.g. `--rewrite`, `--det-opt`, `--profile`.
- Outputs are written under `<base>/P*/output/` and aggregated into `results-*.tsv`.
- See `references/full-context.md` for the full CLI map and artifact names.

## References
- `references/full-context.md`
- `README.eval.md`
- `README.rewrite.impl.md`
- `docs/USAGE.md`

## Source references
- [problog-benchmark/side_channel_full.py](problog-benchmark/side_channel_full.py)
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)

## Related commits
- (no git history yet; uncommitted/new file)
