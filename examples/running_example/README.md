# Running Example (Simple Reachability)

This folder contains a minimal Souffle case that demonstrates how to compile and run a Datalog program with this fork.

## What this example does
The program computes reachability (`path`) from directed edges (`edge`).

Files:
- `compute.dl`: Datalog program
- `input/edge.facts`: input edges
- `input/edge.prob`: optional probabilities aligned with `edge.facts`
- `run.sh`: compile + run helper script
- `output/`: generated results (ignored by git)

## Quick start
Prerequisite: build/install the `souffle` compiler from this repo. Ensure it is on your `PATH` or set `SOUFFLE_BIN` (see below). This fork requires `--online`, and `run.sh` enforces it.

```
./run.sh
```

Results are written to `output/` (look for `path.*`).

## Step-by-step workflow
1) Inspect the program in `compute.dl`.
2) Run the example:
   ```
   ./run.sh
   ```
3) Modify input:
   - Edit `input/edge.facts` (add/remove edges).
   - Facts are tab-separated by default (use `\t`, not spaces).
   - If you keep `input/edge.prob`, keep the same number of lines as `edge.facts`.
4) Run again and compare results in `output/`.
5) Modify the program:
   - Example: add a base case `path(x,x).` to include self-reachability.
   - Example: add a probabilistic rule prefix `0.7::path(x, y) :- edge(x, y).`
   - Example: add evidence `evidence(path(1,4), true).`
   - Re-run `./run.sh` to see updated outputs.

## Notes on probabilities
- This fork reads optional `<rel>.prob` files alongside `<rel>.facts`.
- If a `.prob` file is missing, probabilities default to `1.0`.
- For deterministic runs, you can delete `input/edge.prob`.
- Rule probability prefixes are supported in this fork. Example:
  ```
  0.7::path(x, y) :- edge(x, y).
  ```
  This attaches a probabilistic coin to the rule application.

## Evidence
You can add evidence directives directly in the `.dl` program:
```
evidence(path(1,4), true).
evidence(path(1,5), false).
```
Evidence atoms must match declared relations and arity.

## Important options
### Souffle compiler options (the `souffle` command)
- `-o <bin>`: output binary name.
- `-F <dir>` / `-D <dir>`: default input/output directories baked into the binary.
- `--online`: enable the online incremental CLI in the generated binary (required in this fork).
- `--full-only`: generate a full-only binary (disable incremental pipeline; still compiled with `--online` in this fork).
- `--verbose`: show compiler diagnostics.

### Generated binary options (the compiled program)
- `-F <dir>` / `-D <dir>`: input/output directories at runtime.
- `-d, --derv-only`: only compute the derivation graph.
- `-p <file>`: write profiling info.
- `-k <bdd|sdd>`: select BDD or SDD backend (SDD requires SDD++ install).
- `--dumpdot`, `--dumpjson`, `--dumpstat`: debug outputs (fork-specific).
- `-m, --setmode <inc-naive|inc-regional|full-hard|full-soft|full|elastic>`: incremental mode (when compiled with `--online`).

## Script customization
The `run.sh` script supports environment overrides:
- `SOUFFLE_BIN`: path to the `souffle` compiler.
- `SOUFFLE_COMPILE_OPTS`: extra compile flags (e.g., `--verbose`). `--online` is enforced.
- `SOUFFLE_RUN_OPTS`: extra runtime flags (e.g., `-p run.log`).

Example:
```
SOUFFLE_COMPILE_OPTS="--online" SOUFFLE_RUN_OPTS="-p run.log" ./run.sh
```
