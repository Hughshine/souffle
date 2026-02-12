# P15 FC-only Speedup Summary (full vs inc)

Source: `/home/hugh/research/datalog/souffle/archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/P15/output/per_stage_compare.tsv`
Canonical batch note: `archive/2026-02-11/README.md`

Successful deltas: 19 / 25 (6 timed out and were excluded).

## Overall (FC stage only)
- Full FC mean: 5.782s
- Inc-naive FC mean: 0.335s → **17.26×** speedup (≈ **-94.2%**)
- Inc-regional FC mean: 0.236s → **24.45×** speedup (≈ **-95.9%**)

Medians (per-delta speedup):
- Inc-naive: **18.34×**
- Inc-regional: **23.42×**

## By mix label (ratio-of-means: full_fc_mean / inc_fc_mean)
- mix-d0-i100 (insert-heavy), n=5: **naive 18.47×**, **regional 34.16×**
- mix-d25-i75, n=4: **naive 14.74×**, **regional 20.68×**
- mix-d50-i50, n=3: **naive 16.09×**, **regional 23.72×**
- mix-d75-i25, n=2: **naive 18.08×**, **regional 23.01×**
- mix-d100-i0, n=5: **naive 19.61×**, **regional 21.77×**

## Takeaway (FC-only)
Inc-regional consistently outperforms inc-naive on FC time, with the **largest relative gain in insert-heavy workloads (mix-d0-i100)**.

## Timed-out deltas (excluded)
- mix-d25-i75: sample-2
- mix-d50-i50: sample-1, sample-4
- mix-d75-i25: sample-1, sample-3, sample-4

## Source references
- [archive/README.md](archive/README.md)
- [archive/2026-02-11/README.md](archive/2026-02-11/README.md)

## Related commits
- `UNCOMMITTED` — docs(historical): archive FC-only speedup snapshot note under docs/historical
