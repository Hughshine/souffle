# Language Examples

This page introduces the small probabilistic Datalog fragment used by this
artifact. The intended reader is an evaluator who wants to inspect a complete
program before running the larger `problog-benchmark` cases.

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
`mapping` maps each secret value to an observable event. The probabilistic
relation `monitor` says whether the observation is visible.

```souffle
leak(Observation) :- secret(Value), mapping(Value, Observation).
explained(Observation) :- leak(Observation), monitor(Observation).
query(explained(Observation)).
```

The query asks for the whole `explained` relation, not one selected tuple. The
expected results are:

```text
explained("cache-hit")  = 0.72 * 0.91 = 0.6552
explained("cache-miss") = 0.18 * 0.85 = 0.153
```

This shape resembles the side-channel benchmark at a small scale:
probabilistic inputs feed deterministic propagation rules, then the solver
computes probabilities for all requested output tuples.

## Mini Taint Case

Source: [../tests/regression/cases/language_taint_mini/compute.dl](../tests/regression/cases/language_taint_mini/compute.dl)

This program propagates taint from probabilistic sources through probabilistic
flow edges:

```souffle
tainted(Y) :- tainted(X), flow(X,Y), !sanitizer(Y).
0.80::alarm(Y) :- tainted(Y), sink(Y).
query(alarm(Value)).
```

The query asks for the whole `alarm` relation. The deterministic relation
`sanitizer` blocks the `parser` node, so the path `user -> parser -> network`
does not contribute. The expected output has two tuples:

```text
alarm("cache")   = 0.80 * (0.55 * 0.66) = 0.2904
alarm("network") = 0.80 * (1 - (1 - 0.55*0.66*0.74)
                              * (1 - 0.21*0.59*0.64))
                 = 0.26129241
```

The example covers recursion, deterministic negation, probabilistic facts, and
a probabilistic rule. This shape exercises the rewrite path used for workloads
whose rules have probabilities.

## Mini Symbolization Case

Source: [../tests/regression/cases/language_symbolization_mini/compute.dl](../tests/regression/cases/language_symbolization_mini/compute.dl)

This program mirrors the symbolization benchmark style: input facts describe
objects, object classes, and numeric attributes. A first rule joins symbolic
object names with their numeric points. A second rule computes the requested
total for each selected class:

```souffle
selected_point(Kind, Points) :-
    data_point(Obj, Points),
    object_kind(Obj, Kind).

object_total(Kind, Total) :-
    selected(Kind),
    Total = sum Points : { selected_point(Kind, Points) }.

query(object_total(Kind, Total)).
```

The query asks for the whole `object_total` relation. In this small example the
selected classes are `widget` and `other`, so the expected results are:

```text
object_total("widget",15) = 0.82 * 0.77 * 0.69 = 0.435666
object_total("other",10)  = 0.51
```

The example covers `symbol` attributes, numeric attributes, string constants,
and the supported single-relation aggregate pattern used by the current
pipeline. It is not meant to document a general aggregate semantics.

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
