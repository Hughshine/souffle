# Documentation Index

Read these documents in order when evaluating or modifying the incremental AE
artifact.

1. [../README.md](../README.md): branch scope, build, benchmark handoff.
2. [USAGE.md](USAGE.md): compiler/runtime flags and interactive commands.
3. [ARCHITECTURE.md](ARCHITECTURE.md): end-to-end control flow.
4. [topics/pipeline/README.dred.md](topics/pipeline/README.dred.md): DRed and graph delta flow.
5. [topics/pipeline/README.inc.region.md](topics/pipeline/README.inc.region.md): regional incremental forward compilation.
6. [topics/evaluation/README.artifact.inc.md](topics/evaluation/README.artifact.inc.md): side-channel AE workflow.
7. [TESTING.md](TESTING.md): regression command set.
8. [topics/testing/README.regression.md](topics/testing/README.regression.md): maintained case list.
9. [SECURITY.md](SECURITY.md): dependency and artifact hygiene.
10. [RUNBOOK.md](RUNBOOK.md): compact local run guide.
11. [../AGENTS.md](../AGENTS.md): agent constraints.

## Examples

- [../examples/cycle_repair_path/README.md](../examples/cycle_repair_path/README.md): small
  interactive path program for observing recursive delete/rederive cycle repair
  with `--dump=dot`.

## Source Entry Points

- [../src/MainDriver.cpp:636](../src/MainDriver.cpp#L636): compiler-facing incremental options.
- [../src/synthesiser/Synthesiser.cpp:673](../src/synthesiser/Synthesiser.cpp#L673): generated pipeline call.
- [../src/problog/Pipeline.cpp:923](../src/problog/Pipeline.cpp#L923): baseline graph and runtime pipeline.
- [../src/include/souffle/cli/Executor.h:234](../src/include/souffle/cli/Executor.h#L234): incremental commit path.
- [../src/include/souffle/problog/ForwardCompilation.h:517](../src/include/souffle/problog/ForwardCompilation.h#L517): naive incremental forward compilation.
- [../src/include/souffle/problog/ForwardCompilation.h:1967](../src/include/souffle/problog/ForwardCompilation.h#L1967): regional forward compilation.
- [../tests/regression/CMakeLists.txt:22](../tests/regression/CMakeLists.txt#L22): regression suite.
