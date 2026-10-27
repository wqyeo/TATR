# Documentation

- [Task workflows and examples](usage.md)
- [Configuration and CLI reference](configuration.md)
- [Build and installation](building.md)
- [Build destinations and file operations](file-workflows.md)
- [Node.js/PHP, archives and database workflow verification](workflow-verification.md)
- [Development checks and changelog maintenance](development.md)
- [Packaging](packaging.md) and [0.2.0a release plan](release-0.2.0a.md)
- [Changelog](../CHANGELOG.md)
- [Verification and compatibility evidence](verification.md)
- [Next increment status](next-increment.md)
- [Run-completion hooks](completion-hooks.md)
- [SMTP email actions](smtp.md)
- [Notification design history](notifications.md)

## Project layout

- `src/main.cpp`: native command-line entry point and error reporting.
- `src/cli.*`: CLI parsing and output.
- `src/config.*`: configuration model and validation.
- `src/environment.hpp`: portable override names and environment map type.
- `src/dotenv.*`: bounded dotenv file loading and parsing.
- `src/logging.*`: synchronous action records and console/file output.
- `src/planner.*`: dependency validation and deterministic planning.
- `src/runner.*`: sequential task execution and failure reporting.
- `src/process.*`: native Windows/POSIX process launching, private input and waiting.
- `src/smtp.*`: bounded MIME messages and external SMTP-client transport.
- `third_party/tomlc17/`: pinned parser source, provenance, and license.
- `tests/`: CLI checks using CTest and CMake, without a test framework dependency.
- `CMakeLists.txt` and `CMakePresets.json`: build and installation setup.
- `mise.toml`: optional developer tooling and build tasks.
- `examples/tatr.toml`: supported configuration example.
- `examples/.env.example`: public dotenv values for the workflow example.
- `examples/file-operations.toml`: runnable copy/move example using CMake.
- [Design notes](design.md): problem statement, principles, and open decisions.
- [Parser and schema decisions](configuration-decisions.md): approved decisions and comparison.
- [Dependency-planning increment](dependency-planning.md): completed approved scope.
- [Command-execution increment](command-execution.md): completed approved scope.
- [Safety-hardening increment](safety-hardening.md): completed approved scope.
- [Working-directory increment](task-working-directories.md): completed approved scope.
- [Environment increment](task-environment.md): completed approved scope.
- [Runtime-argument increment](runtime-arguments.md): completed approved scope.
- [Dotenv increment](dotenv-files.md): completed approved scope.
- [Logging increment](action-logging.md): completed approved scope.
- [glibc compatibility increment](glibc-linux.md): completed locally; unpublished candidate.
- [Changelog/README increment](changelog-readme.md): completed documentation scope.
- [Release checklist](release-checklist.md): first-use evidence and remaining decisions.
- [Packaging](packaging.md): local archive builds, verification and publication procedure.
- [Development checks](development.md): local gates and proposed CI workflow.
- [Increment status](next-increment.md): completed scope and review status.

Each increment is planned and reviewed before implementation. First-use workflows
have been exercised on Windows and Alpine Linux; the checklist records remaining
release decisions. Performance benchmarking is deferred;
parallelism and caching remain future work.
