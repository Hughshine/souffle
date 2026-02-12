# TODO

- Investigate `P12` compare-all timeout for `mix-d25-i75` sample-4 (inc-naive + inc-regional exit=124).
  - Delta: `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/P12/delta/mix-d25-i75_4.txt`
  - Output: `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/P12/output/delta-mix-d25-i75/sample-4/`
  - Meta JSON: `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/P12/output/delta-mix-d25-i75/sample-4/delta-mix-d25-i75-4.json`
Another tricky part is DD dynamic reordering: BDD libraries use periodic
reordering to reduce formula blow-up, but reordering itself is expensive.
Both `inc-naive` and `inc-regional` reuse old BDDs and variable ordering.
`inc-naive` may still trigger reordering more often because it performs more
formula operations. We need a clear policy for when incremental turns should
disable or re-enable dynamic reordering.

## Source references
- [archive/README.md](archive/README.md)
- [archive/2026-02-11/README.md](archive/2026-02-11/README.md)

## Related commits
- `UNCOMMITTED` — docs(historical): archive pending follow-up notes under docs/historical
