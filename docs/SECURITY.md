# Security

## Scope
This is a research fork intended for local experiments. There is no production
security policy defined in this repo yet.

## Secrets and Configuration
- Do not commit secrets, credentials, or private datasets.
- Use environment variables for local configuration; `.env` is not auto-loaded.

## Data Handling
- Treat input facts and output logs as potentially sensitive.
- Keep generated artifacts out of commits (see `README.git.md`).

## Dependency Hygiene
- Dependency install scripts live under `sh/setup/`.
- CUDD is required for the BDD backend; SDD is optional for `-k sdd`.
- Review and pin dependency versions when preparing releases.

## Disclosure / Reporting
TODO: define a security contact and disclosure process for this fork.

## Pre-Release Checklist
- Verify dependency sources and versions.
- Ensure no secrets or datasets are present in the working tree.
- Update docs and run the recommended verification steps.
