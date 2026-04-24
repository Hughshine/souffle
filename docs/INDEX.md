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
9. [RUNBOOK.md](RUNBOOK.md): compact local run guide.
10. [../AGENTS.md](../AGENTS.md): agent constraints.

## Source Entry Points

- [../src/MainDriver.cpp:623](../src/MainDriver.cpp#L623): compiler-facing incremental options.
- [../src/synthesiser/Synthesiser.cpp:673](../src/synthesiser/Synthesiser.cpp#L673): generated pipeline call.
- [../src/problog/Pipeline.cpp:914](../src/problog/Pipeline.cpp#L914): baseline graph and runtime pipeline.
- [../src/include/souffle/cli/Executor.h:233](../src/include/souffle/cli/Executor.h#L233): incremental commit path.
- [../src/include/souffle/problog/ForwardCompilation.h:512](../src/include/souffle/problog/ForwardCompilation.h#L512): naive incremental forward compilation.
- [../src/include/souffle/problog/ForwardCompilation.h:1960](../src/include/souffle/problog/ForwardCompilation.h#L1960): regional forward compilation.
- [../tests/regression/CMakeLists.txt:22](../tests/regression/CMakeLists.txt#L22): regression suite.
