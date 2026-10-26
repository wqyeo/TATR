# Next increment proposal: C++ safety hardening and developer checks

Status: approved, implemented, and verified.

The user approved the seven steps below. See [the development workflow](development.md)
and [verification results](verification.md) for implementation details and evidence.
The subsequent [task working-directory increment](task-working-directories.md)
was reviewed separately.

## Outcome

Tighten native resource handling and establish reproducible local checks without
adding production dependencies or requiring CI infrastructure.

## Proposed steps

1. Restrict Windows child-handle inheritance to the duplicated standard handles
   using STARTUPINFOEX and an explicit handle list. Keep attribute storage and
   handles under RAII, including failure paths and absent standard handles.
   Add a Windows regression proving an unrelated inheritable handle is excluded
   while standard I/O still works.
2. Introduce a documented 1 MiB configuration-file limit before allocating parser
   input; test the boundary and rejection without executing tasks. This bounds
   source input size, not total parser/graph memory usage.
3. Add opt-in strict compiler checks for project-owned C++ targets: warnings as
   errors with MSVC and GCC/Clang, plus a reviewed selection of conversion and
   shadowing diagnostics where supported. Resolve findings without blanket
   suppressions; keep vendored code outside the project's warning policy.
4. Add an isolated Linux Clang ASan + UBSan build/test preset or documented
   container task. Instrument the vendored parser as well as project code and
   enable failing sanitizer diagnostics. Keep release builds unsanitized. Verify
   actual failures propagate through the CTest harness. Windows MSVC ASan is a
   follow-up unless its local toolchain/runtime compatibility is verified.
5. Add pinned clang-tidy and clang-format tooling, configuration, and mise tasks.
   Run focused static-analysis checks for defects, dangling references, ownership,
   and unsafe conversions against a compilation database. Review each enabled
   check; document narrow native-API exceptions. Exclude third_party from analysis
   and formatting. Establish a formatting baseline for project-owned C++ only.
6. Document a local edit -> format -> strict build -> test -> analysis -> sanitizer
   workflow. Draft a GitLab CI proposal using the same commands, with one Linux
   quality job and a separate sanitizer job. Identify native Windows/macOS jobs
   as dependent on runner availability. Do not activate pipelines, provision
   runners, or change remote project settings in this increment.
7. Run Windows and Linux regression checks and the new local quality gates;
   record tool versions, findings, exceptions, release-size changes, and platform
   gaps. Present results for review; commit/push only when authorized.

## Scope boundary

No cwd feature, new execution backend abstraction, inheritance hierarchy, new
production dependency, cancellation/process-tree management, or remote CI changes.
Static analysis and sanitizers improve detection; they do not prove C++ memory
safety or SOLID compliance. Fuzzing and MSVC /analyze can be evaluated separately
after the first gates are stable.

## Tool references

- [Windows handle allowlist](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute)
- [clang-tidy](https://clang.llvm.org/extra/clang-tidy/)
- [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
- [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
- [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [MSVC AddressSanitizer](https://learn.microsoft.com/en-us/cpp/sanitizers/asan)
- [GitLab hosted runners](https://docs.gitlab.com/ci/runners/hosted_runners/)
