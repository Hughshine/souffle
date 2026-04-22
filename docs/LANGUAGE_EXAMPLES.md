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

The examples use `_` in `query(...)`. `_` is an unnamed wildcard variable. A
query such as `query(explained(_)).` asks for every tuple in the one-column
relation `explained`. A two-column relation uses one wildcard per column, for
example `query(object_total(_, _)).`.

## Mini Side-Channel Case

Source: [../tests/regression/cases/language_side_channel_mini/compute.dl](../tests/regression/cases/language_side_channel_mini/compute.dl)

This program is a two-event toy model. A hidden value may be `admin` or `guest`.
If the hidden value is `admin`, the deterministic table `mapping` says that the
observable event is `cache-hit`. If the hidden value is `guest`, the event is
`cache-miss`. A second probabilistic relation, `monitor`, says whether that
event is visible to the observer.

The input probabilities are:

```text
secret("admin")      = 0.72
secret("guest")      = 0.18
monitor("cache-hit") = 0.91
monitor("cache-miss")= 0.85
```

The first rule computes which observation each possible secret would produce.
The second rule says that an observation is explained only when the secret
would produce it and the monitor sees it.

```souffle
leak(Observation) :- secret(Value), mapping(Value, Observation).
explained(Observation) :- leak(Observation), monitor(Observation).
query(explained(_)).
```

The query asks for the whole `explained` relation, not one selected tuple. The
runtime therefore reports both possible observations:

```text
explained("cache-hit")  = 0.72 * 0.91 = 0.6552
explained("cache-miss") = 0.18 * 0.85 = 0.153
```

This shape resembles the side-channel benchmark at a small scale:
probabilistic inputs feed deterministic propagation rules, then the solver
computes probabilities for all requested output tuples.

## Mini Taint Case

Source: [../tests/regression/cases/language_taint_mini/compute.dl](../tests/regression/cases/language_taint_mini/compute.dl)

This program is a small data-flow analysis. A value is tainted when it starts
from a probabilistic source or when taint flows into it along a probabilistic
edge. A deterministic `sanitizer` relation blocks propagation into selected
nodes. A deterministic `sink` relation marks the nodes that should raise an
alarm if they become tainted.

The relevant input probabilities are:

```text
source("user")        = 0.55
source("config")      = 0.21
flow("user","cache")  = 0.66
flow("cache","network") = 0.74
flow("config","log")  = 0.59
flow("log","network") = 0.64
```

`parser` is listed in `sanitizer.facts`, so the path
`user -> parser -> network` is blocked even though the corresponding flow facts
exist. The recursive rule below computes transitive taint flow through the
remaining graph. The probabilistic rule says that a tainted sink raises an
alarm with probability `0.80`.

```souffle
tainted(Value) :- source(Value).
tainted(Y) :- tainted(X), flow(X,Y), !sanitizer(Y).
0.80::alarm(Y) :- tainted(Y), sink(Y).
query(alarm(_)).
```

The query asks for the whole `alarm` relation. The deterministic relation
`sanitizer` blocks the `parser` node, so the path `user -> parser -> network`
does not contribute. The expected output has two tuples. `cache` has one
derivation path. `network` has two independent derivation paths: one through
`cache` and one through `log`.

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

The input facts say that `obj-a`, `obj-b`, and `obj-c` are widgets with point
values `4`, `6`, and `5`. `obj-noise` belongs to class `other` and has point
value `10`. The `data_point` facts are probabilistic:

```text
data_point("obj-a",4)     = 0.82
data_point("obj-b",6)     = 0.77
data_point("obj-c",5)     = 0.69
data_point("obj-noise",10)= 0.51
```

```souffle
selected("widget").
selected("other").

selected_point(Kind, Points) :-
    data_point(Obj, Points),
    object_kind(Obj, Kind).

object_total(Kind, Total) :-
    selected(Kind),
    Total = sum Points : { selected_point(Kind, Points) }.

query(object_total(_, _)).
```

The query asks for the whole `object_total` relation. The current aggregate
support records the aggregate witnesses as dependencies of the derived total.
For this example, the `widget` total depends on all three widget point facts,
and the `other` total depends on the one `other` point fact:

```text
object_total("widget",15) = 0.82 * 0.77 * 0.69 = 0.435666
object_total("other",10)  = 0.51
```

This example mainly illustrates symbol-valued facts, numeric attributes,
string constants, and object-property joins. The aggregate is included only as
the supported single-relation pattern used by the current pipeline; it is not a
complete specification of aggregate semantics.

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
