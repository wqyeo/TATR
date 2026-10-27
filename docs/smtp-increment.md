# Structured SMTP implementation increment

Status: separately approved and implemented locally on 2026-10-27. Final
verification passed; see [SMTP usage](smtp.md) and
[verification results](verification.md). This file preserves the reviewed scope.
Commit/push and publication require separate authorization.

## Full next-increment plan

1. Add a typed smtp action, mutually exclusive with exec, to hooks and ordinary
   tasks. Reuse completion selection, target filters and warn/fail policy. Ordinary
   SMTP tasks participate in depends ordering; reject nonempty runtime arguments
   before prerequisites. Keep task/hook cwd, env and env_file with their existing
   semantics. Use composition/value types and an explicit action alternative,
   not an inheritance/plugin framework. Preserve old configurations and direct
   command behavior.
2. Validate the entire schema before execution. smtp has host (DNS name, IPv4 or
   bracketed IPv6, without URL/path/userinfo/controls), required integer port
   1..65535, tls (starttls or implicit), from (one ASCII mailbox), to (1..32
   distinct ASCII mailboxes), subject (nonempty UTF-8, at most 512 bytes, no
   controls), optional body or body_file (mutually exclusive), optional client
   (default curl), optional ca_file, connect_timeout_seconds (default 10) and timeout_seconds
   (default 30). Each timeout is an integer 1..3600; connection timeout must not
   exceed total timeout. Reject unknown fields and header/option injection. Limit
   each mailbox to 254 bytes and reject display-name/list/comment syntax. client
   follows existing native executable/path lookup rules; reject NUL/empty values.
   body_file and ca_file resolve from the TOML directory, independent of cwd.
   ca_file selects a PEM trust bundle; omitted uses curl's configured platform
   trust. Never disable hostname/certificate verification. Explicit tasks
   require content; hooks without content get the standard original-run summary.
3. Add optional smtp.auth with username_env and password_env, both required
   together and portable environment names. Omitting auth explicitly means an
   unauthenticated relay. Resolve values immediately before sending, using the
   action's inherited environment, dotenv and inline overrides; preserve reserved
   hook context precedence. Missing/empty credentials fail without printing
   values. Bound each credential to 4096 UTF-8 bytes, reject NUL/control characters
   and reject colon in usernames for curl's user/password representation. Do not
   add plaintext password fields, OAuth, credential discovery or general templates.
4. Generate a plain-text MIME email with From/To, encoded UTF-8 Subject, Date,
   Message-ID and explicit charset/transfer encoding. Standard hook bodies contain
   target, original outcome/status, failed task and duration; never child output,
   argv, exception text or environment values. Permit an explicit UTF-8 body string
   or file, at most 64 KiB before encoding, with optional initial BOM for files;
   reject NUL, invalid UTF-8 and lone CR, and normalize LF/CRLF. Read files just
   before execution, allowing prerequisites to generate them. Use encoded subject
   folding, quoted-printable body encoding, CRLF output and bounded generated
   message size (512 KiB). No HTML, attachments, display names or SMTPUTF8 mailboxes.
5. Invoke an external curl 8.0+ executable with SMTP/SMTPS and TLS capabilities;
   record tested versions and check compatibility when SMTP is selected. No curl
   requirement for builds or non-SMTP workflows. Disable implicit curl config,
   traces, retries, ambient proxies and URL globbing; require TLS/certificate verification with no
   insecure option. Use typed construction and escaped curl configuration through
   private stdin, capped at 64 KiB. Keep credentials out of argv, log records and
   files. Upload the generated message from an exclusively created private
   temporary directory/file, with restrictive POSIX permissions/Windows access
   and checked writes/close plus scoped cleanup. Do not put credentials in it.
   Treat any recipient rejection as failure. Apply both configured timeouts to
   the transport; these do not establish a deadline for TATR preparation or normal
   commands. Document the 8.0+ floor and actual SMTP/TLS capability requirement;
   unavailable protocol/TLS support must fail clearly when selected. Do not run
   client probes from list/plan or for unselected SMTP actions.
6. Add narrowly scoped process support for supplied stdin bytes and for routing
   SMTP-client stdout/stderr to the OS null device. Ordinary commands retain
   inherited I/O; do not implement general capture or teeing. Preserve existing
   native process launch and descriptor/
   handle restrictions. Own pipe ends and input storage with RAII; use a scoped
   writer with checked errors and joining on every launch/wait path, handle early
   child exit/broken pipes without killing TATR, and prevent inherited write ends
   keeping stdin open. Review POSIX SIGPIPE handling and Windows handle allowlists.
   Resolve partial writes and thread-start/launch failures without abandoned
   children or threads. Do not add general capture, cancellation or timeouts to
   ordinary commands. If this cannot be implemented safely within that scope,
   return the revised design for review before changing the transport approach.
   Opening/closing null handles and temporary-file cleanup must also be checked;
   cleanup failures are runner errors subject to the existing task/hook policy,
   and must not obscure an earlier sending failure.
7. Keep SMTP lifecycle records in task/hook events, distinguishing successful
   sending, client failure and runner preparation errors. Successful sending is
   logged as client/server acceptance, without claiming recipient delivery. Exclude message bodies,
   credentials and generated client config from TATR diagnostics. Use controlled
   diagnostics based on native/curl status; suppress raw SMTP client/server output
   rather than promising its redaction. Ordinary child output remains inherited,
   and privileged local access is not a general secrecy boundary. Report failures
   through the existing task/hook policy. One attempt; no delivery spool or
   retry. SMTP success means server acceptance, not inbox delivery. Errors may
   follow partial acceptance, so retrying can duplicate messages.
8. Test schema, message bytes/encoding, safe client configuration, environment
   precedence, stdin early-close/partial-write/launch errors, cleanup and secret
   exclusion with a fake native client. Then use disposable local SMTP fixtures
   with synthetic credentials and recipients for sending, authentication/recipient
   rejection, connection refusal, STARTTLS/implicit TLS, certificate rejection and
   timeouts. Verify task dependencies, hook matching and both failure policies.
   Include no-side-effect list/plan checks and custom CA trust tests.
   Send no real mail. Run Windows strict and available Linux GCC, Clang/static
   analysis and ASan/UBSan checks; report macOS/provider-specific limits explicitly.
9. Add runnable TOML/public dotenv examples, configuration and installation/client
   capability instructions, failure/privacy/delivery limitations, verification
   evidence and an Unreleased changelog entry. Report results for review. Keep
   built-in SMTP/TLS, OAuth flows, attachments, retries, spooling, benchmarks,
   release changes and commit/push outside this increment.

## Proposed configuration (not supported yet)

```toml
version = 1

[tasks.deploy]
exec = ["deployment-tool", "apply"]

[[hooks]]
name = "deployment-email"
when = "always"
targets = ["deploy"]
on_error = "warn"
env_file = ".env.notifications"

[hooks.smtp]
host = "smtp.example.com"
port = 587
tls = "starttls"
from = "deploy@example.com"
to = ["operations@example.com"]
subject = "Deployment result"
connect_timeout_seconds = 10
timeout_seconds = 30
# ca_file = "certificates/internal-ca.pem" # optional private CA trust bundle

[hooks.smtp.auth]
username_env = "SMTP_USERNAME"
password_env = "SMTP_PASSWORD"
```

The body defaults to the original run summary; no wrapper script is needed.
An ordinary task uses [tasks.NAME.smtp] with the same fields and explicit body or
body_file. A curl executable and its CA trust setup remain external requirements.
The minimum client version is a proposed support floor, not a claim that older
curl cannot send SMTP. Private fixture infrastructure is for tests only.

## Completion criteria

- One TOML hook can send the original deployment summary after success or failure
  without a wrapper script. An explicit SMTP task can send supplied message content.
- Configurations without SMTP retain their current behavior and dependencies.
- No real SMTP credentials or externally delivered emails are used in tests.
- Authentication is absent from generated argv, logs and temporary files; raw
  SMTP diagnostics are suppressed. Explicit user message content is user-controlled.
- Both TLS modes, custom/default trust behavior, failure policies and cleanup are
  covered, with exact tested platforms/clients and honest coverage limits recorded.

## References and review boundary

See the [notification roadmap](notifications.md) and official
[curl manual](https://curl.se/docs/manpage.html). Final option selection and MIME
encoding will be checked against primary documentation during implementation.
SMTP was approved separately after the completion-hook increment. No further
feature or release action is authorized by this archived plan.
