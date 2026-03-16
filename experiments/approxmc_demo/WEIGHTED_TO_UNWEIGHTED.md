# Weighted -> Unweighted Conversion (This Demo)

This note documents how `weighted_conversion.cpp` transforms a weighted CNF into an unweighted CNF that can be counted by ApproxMC.

## Goal

Input:
- CNF `F` over variables `x1..xn`
- sampling set `S` (default: all vars)
- literal weights `w(lit)` and optional global `multiplier`

Output:
- unweighted CNF `F'`
- expanded sampling set `S'`
- scaling metadata: `multiplier`, `divideExp`

Final reconstruction used by the demo:

```text
weightedEstimate = unweightedEstimate * multiplier / 2^divideExp
```

where:

```text
unweightedEstimate = cellSolCount * 2^hashCount
```

## Step-by-Step Pipeline

1. Parse and canonicalize CNF
- remove duplicate literals in a clause
- remove tautological clauses
- if an empty clause appears, mark `unsat=true`

2. Preprocess (when `config.preprocess=true`)
- initialize unit queue from unit clauses
- also push implied units from zero weights:
  - `w(x)=0  => force x=false`
  - `w(~x)=0 => force x=true`
- propagate each forced literal through CNF
- for each forced literal `l`, accumulate `multiplier *= w(l)`
- drop forced vars from sampling set and weight map
- detect conflicts (`x=true` and `x=false`) as UNSAT

3. Keep only sampling-relevant weights
- remove weights for vars not in current sampling set

4. Complete missing polarity and normalize
- if only one side is provided, fill the other by complement:
  - `w(~x)=1-w(x)` or `w(x)=1-w(~x)`
- drop trivial `(w(x), w(~x))=(1,1)` pairs
- normalize each variable pair to sum to 1:
  - `total = w(x)+w(~x)`
  - `w(x) /= total`, `w(~x) /= total`
  - `multiplier *= total`

5. Compute tilt and optional guard
- tilt is computed on remaining positive weights:
  - `tilt = maxPositiveWeight / minPositiveWeight`
- if `tilt > tiltMax`:
  - mark `tiltViolated=true`
  - if `failOnTilt=true`, return invalid with message

6. Quantize normalized weights
- each weight is rounded to dyadic form with chosen precision `p`:
  - round(`w * 2^p`) -> integer `m`
  - strip powers of two so representation is `m / 2^k`
- record diagnostic errors:
  - `maxQuantAbsError`
  - `maxQuantRelError`

7. Encode each weighted variable with chain CNF
- for each non-trivial dyadic weight, add auxiliary vars/clauses via chain encoding
- also encode complement side and deduplicate identical clauses
- extend sampling set with introduced auxiliaries
- accumulate denominator exponent:
  - `divideExp += k` for each encoded variable

8. Return unweighted artifact
- `numVars`, `clauses`, `samplingSet`
- `multiplier`, `divideExp`
- diagnostics (`addedVars`, `addedClauses`, tilt, quantization stats, forced assignments)

## Why This Can Blow Up

The conversion is correct but not size-preserving.

Where expansion comes from:
- each non-trivial weighted variable is replaced by chain-encoding gadgets
- gadgets add auxiliary variables and clauses
- introduced auxiliaries are added to sampling set as needed

Typical growth trend:
- roughly proportional to `(#weighted variables) * (quantization precision)`
- worse when many weights are non-dyadic (require richer encoding)

Operational consequence:
- the reduced unweighted CNF can become much larger than input
- ApproxMC then pays higher SAT cost (more conflicts/restarts/rounds)
- overall weighted pipeline may be slower than DD-based WMC on many instances

## Exactness vs Approximation

Two approximation sources are separate:

1. ApproxMC counting error (`epsilon`, `delta`)
- applies to the unweighted count of converted CNF.

2. Weight quantization error
- from finite precision dyadic rounding in step 6.
- reported by `maxQuantAbsError` / `maxQuantRelError`.

So `epsilon/delta` do not fully describe total weighted error when quantization is non-zero.

## Key Data Structures

- `WeightedCNFInput` / `UnweightedCNFResult`: [WeightedConversion.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/approx/WeightedConversion.h)
- conversion implementation: [WeightedConversion.cpp](/home/hugh/research/datalog/souffle/src/problog/approx/WeightedConversion.cpp)
- wrapper using ApproxMC + reconstruction:
  [weighted_appmc.cpp](/home/hugh/research/datalog/souffle/experiments/approxmc_demo/weighted_appmc.cpp)
