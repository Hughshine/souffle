# Language Examples

This page introduces the small probabilistic Datalog fragment used by the
artifact. The intended reader is an evaluator who wants to inspect a program
before running the larger `problog-benchmark` cases.

The examples below are also regression tests. CTest compiles each program,
runs exact inference with and without `--rewrite`, and checks that both runs
produce the same output tuple probabilities.

## Files

Each example has the same layout:

```text
tests/regression/cases/<case>/
  compute.dl
  input/<relation>.facts
  input/<relation>.prob
```

Facts are tab-separated tuples. A matching `.prob` file gives one probability
per fact line. If a relation has no `.prob` file, the runtime treats its facts
as deterministic.

## Mini Side-Channel Case

Source: [../tests/regression/cases/language_side_channel_mini/compute.dl](../tests/regression/cases/language_side_channel_mini/compute.dl)

This program models a secret-dependent observation. The probabilistic input
relation `secret` represents possible secret values. The deterministic relation
`mapping` maps each secret value to an observable event. The `monitor` relation
is probabilistic and is also used as evidence:

```souffle
query(explained("cache-hit")).
evidence(monitor("cache-hit"), true).
```

The example covers probabilistic facts, deterministic joins, query selection,
and evidence conditioning. It resembles the side-channel benchmark shape at a
small scale: probabilistic inputs feed deterministic propagation rules before
the solver computes output probabilities.

## Mini Taint Case

Source: [../tests/regression/cases/language_taint_mini/compute.dl](../tests/regression/cases/language_taint_mini/compute.dl)

This program propagates taint from probabilistic sources through probabilistic
flow edges:

```souffle
tainted(Y) :- tainted(X), flow(X,Y), !sanitizer(Y).
0.80::alarm(Y) :- tainted(Y), sink(Y).
```

The example covers recursion, negation over a deterministic input relation,
and a probabilistic rule. This shape exercises the optimized rewrite path used
for workloads with probabilistic rules.

## Mini Symbolization Case

Source: [../tests/regression/cases/language_symbolization_mini/compute.dl](../tests/regression/cases/language_symbolization_mini/compute.dl)

This program computes an aggregate over symbolic object names:

```souffle
selected_point(Kind, Points) :-
    data_point(Obj, Points),
    object_kind(Obj, Kind).

object_total(Kind, Total) :-
    selected(Kind),
    Total = sum Points : { selected_point(Kind, Points) }.
```

The example covers `symbol` attributes, numeric attributes, string constants,
and `sum` aggregates. It matches the symbolization benchmark style: input facts
describe objects and attributes, and the output relation asks for aggregate
properties of selected objects.

## Run the Examples

After building the compiler, run the maintained regression label:

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}
```

To run one example directly:

```bash
python3 tests/regression/run_regression_case.py \
  --case language_taint_mini \
  --souffle-bin build/src/souffle \
  --work-root build/tests/regression
```

The runner compiles `compute.dl`, runs the generated binary once with
`--det-opt`, runs it again with `--det-opt --rewrite`, and compares the two
`facts.prob` files.
