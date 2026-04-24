# Security

## Source References

- [../README.md:1](../README.md#L1): branch scope.
- [topics/evaluation/README.artifact.inc.md:1](topics/evaluation/README.artifact.inc.md#L1): benchmark workflow.
- [../.gitignore:1](../.gitignore#L1): ignored local artifacts.

## Scope

This branch is a research artifact branch, not a production distribution.

## Data Handling

- Do not commit private datasets, generated benchmark outputs, or timing logs.
- Treat fact files, probability files, and JSON logs as experiment data.
- Keep large or machine-local runs outside the repository checkout.

## Dependency Hygiene

- The artifact build depends on the system C++ toolchain, CMake, Flex/Bison,
  Python 3, Readline, and CUDD.
- Pin dependency versions in external artifact instructions when preparing a
  release archive.
