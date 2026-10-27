# Explicit dotenv-file increment

Status: approved, implemented and verified; committed alongside logging in `01566dd`.

The user approved the six steps below. See [the configuration contract](configuration.md)
and [verification results](verification.md) for final behavior and evidence.
First-use/release readiness is the [next proposed increment](next-increment.md).
Performance benchmarking remains deferred.

## Outcome

Allow an executable task to load one explicitly named dotenv file through
`env_file = ".env"`, with deterministic parsing and override precedence.

```toml
[tasks.build]
exec = ["my-build-tool"]
env_file = ".env"
env = { BUILD_MODE = "release" }
```

## Proposed steps

1. Add optional `env_file` as one nonempty string, rejecting NUL and Windows
   drive-relative paths. Resolve relative paths against the TOML file's directory,
   independently of task cwd; accept absolute paths. Reject the field on grouping
   tasks and validate its schema for all tasks before any execution.
2. Read the selected task's file immediately before launching its child, after
   prerequisites, so a prerequisite can generate it. Missing/unreadable files and
   parse failures stop execution with exit 2 and task/file/line context where
   available. Earlier effects remain. List/plan validate TOML fields without
   opening env files; unrelated tasks' files are not read. Do not discover .env
   files automatically or silently ignore missing files.
3. Add a small parser without a production dependency. Require valid UTF-8 with
   optional initial BOM, LF/CRLF lines, and a 1 MiB file limit checked before input
   allocation. Accept blank lines, # comments, optional export followed by
   whitespace, and NAME=value assignments split at the first =, with portable
   names matching the inline env rules. Empty values are valid. Trim surrounding spaces/tabs on unquoted
   values; retain internal whitespace and literal # unless at value start or
   preceded by whitespace. Single-quoted values are literal; double-quoted values
   support only `\\`, `\"`, `\n`, `\r`, and `\t` escapes. After a closing quote, allow only
   whitespace or a comment. Reject unknown escapes, malformed assignments,
   unmatched/multiline quotes, NUL, invalid UTF-8, and duplicate names (Windows
   case-insensitive, POSIX case-sensitive). Do not expand variables, execute
   commands, concatenate quoted fragments, or join physical lines. Diagnostics
   report location and reason without printing file lines or variable values.
4. Merge inherited environment < dotenv file < inline env, with later layers
   taking precedence. Apply native name casing and effective child PATH lookup
   consistently; keep TATR and other tasks' environments unchanged. An empty file
   is valid. Override conflicts within one source remain errors, while inline env
   may intentionally replace a file value, including Windows case variants.
5. Test paths relative to TOML rather than cwd, absolute/Unicode/spaced paths,
   LF/CRLF/BOM, comments/export/quotes/escapes/empty values, duplicate/case rules,
   malformed UTF-8 and syntax, NUL/size limits, missing files, generated files,
   precedence, PATH lookup, isolation, secret-free diagnostics, and list/plan
   avoiding file reads. Retain existing tests; run Windows/Linux GCC/Clang, strict
   builds, formatting/static analysis, and sanitizer checks. Report macOS gaps.
6. Update help, README/configuration docs, and a secret-free .env.example. Ignore
   local .env files while retaining the example. Record verification and release
   sizes, present results for review, and commit/push only when authorized.

## Scope boundary

One explicit file per executable task. No env-file arrays, automatic discovery,
optional/missing-file mode, global dotenv scope, interpolation, physical multiline
values, environment deletion, new production dependency, benchmark framework,
remote CI activation, or release publication.

## Implementation decisions

- dotenv.cpp owns bounded filesystem reads, UTF-8 validation, and line parsing;
  parse errors contain only path/line/reason. No dotenv dependency was added.
- File keys normalize through environment_name_key before merging, giving Windows
  case-insensitive replacement and POSIX case-sensitive behavior. Inline env wins
  without mutating the stored task or inherited process environment.
- The runner loads each task's file after its prerequisites, inside task error
  handling, then reuses the existing native environment and PATH implementation.
- The workflow example explicitly reads the public examples/.env.example; local
  .env and .env.* files are ignored by Git, with .env.example retained.
