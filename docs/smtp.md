# SMTP email actions

Available in 0.2.0a; verification is recorded in [verification.md](verification.md).
The older 0.1.0a archives remain unchanged.

TATR prepares email and invokes an external native curl executable. It does not
link libcurl or a TLS library. No curl installation is needed unless an SMTP
action is selected. The supported client baseline is curl 8.0+ built with SMTP,
SMTPS and TLS; check `curl --version`. Some OS distributions omit protocols.
TATR does not version-probe clients: unsupported protocols/options fail when used.
Configure `client` with an explicit executable path if PATH selects the wrong curl.

## Deployment notifications

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

[hooks.smtp.auth]
username_env = "SMTP_USERNAME"
password_env = "SMTP_PASSWORD"
```

Provide credentials in the explicit dotenv file or inherited environment:

```dotenv
SMTP_USERNAME="your-smtp-account"
SMTP_PASSWORD="replace-with-your-password"
```

Keep the private file out of Git. Existing precedence applies: inherited values,
then dotenv, then inline overrides, then reserved hook context. These values are
child-only. Omitting smtp.auth explicitly selects an unauthenticated relay; it
does not disable encryption. Provider-specific OAuth flows are not implemented.
An account password/app password works only when the provider permits that form
of SMTP authentication.

Without body/body_file, a hook sends a summary such as:

```text
TATR deployment result
Target: deploy
Outcome: child_failure
Status: 1
Failed task: build
Task duration (ms): 1200
```

Hooks use the original run result. They run once after the target's prerequisites
and action, including failure, following the [completion-hook rules](completion-hooks.md).
Default on_error=warn reports email failure without changing deployment status.
With fail, an otherwise successful run returns 1 for client failure or 2 for
preparation/launch/cleanup failure. An already failed deployment retains its
original result. Logging failures still stop subsequent actions with exit 2.

## Explicit email tasks

```toml
version = 1

[tasks.email]
env_file = ".env.notifications"

[tasks.email.smtp]
host = "smtp.example.com"
port = 465
tls = "implicit"
from = "deploy@example.com"
to = ["operations@example.com"]
subject = "Backup completed"
body_file = "messages/backup.txt"

[tasks.email.smtp.auth]
username_env = "SMTP_USERNAME"
password_env = "SMTP_PASSWORD"
```

Use body for a literal UTF-8 string, or body_file for content that a prerequisite
may generate. Ordinary SMTP tasks require one of them; hooks can use either to
replace the default summary. Empty explicit bodies are valid. There is no template
expansion. Ordinary SMTP tasks support depends, description, cwd, env and env_file;
exec and smtp are mutually exclusive. A task with neither action is a group.
Nonempty runtime arguments for SMTP tasks are rejected before prerequisites or
logging initialization. Runtime arguments never go to SMTP hooks.

## Settings and limits

| Field | Contract |
| --- | --- |
| host | Required DNS hostname, strict IPv4 or bracketed IPv6; no URL, userinfo, path, controls or IPv6 zone identifier. |
| port | Required integer 1..65535. |
| tls | Required starttls (mandatory upgrade) or implicit (SMTPS). No plaintext mode or disable-verification switch. |
| from | Required one ASCII dot-atom mailbox, at most 254 bytes (local part at most 64). |
| to | Required 1..32 mailboxes of the same form. Duplicate local-part/exact-case plus case-insensitive-domain addresses are rejected. No display names, address lists, comments or address literals. |
| subject | Required nonempty UTF-8, at most 512 bytes; no C0/DEL/C1 controls. |
| body / body_file | Optional mutually exclusive content source; explicit tasks require one. At most 64 KiB UTF-8 before encoding, excluding an optional initial file BOM. NUL, invalid UTF-8 and lone CR are rejected. LF/CRLF are accepted and normalized. |
| client | Optional native executable name/path; default curl. Existing cwd/PATH lookup rules apply. No shell or PATHEXT expansion. |
| ca_file | Optional PEM trust bundle, resolved from the TOML directory. Without it curl uses its configured platform trust and environment. Hostname/certificate verification remains required. |
| connect_timeout_seconds | Integer 1..3600, default 10; must not exceed total timeout. |
| timeout_seconds | Integer 1..3600, default 30. Applies to curl's transport, not all TATR preparation or ordinary commands. |
| auth.username_env / auth.password_env | Both portable environment-variable names required when auth is present. Values must be nonempty UTF-8, at most 4096 bytes each, without controls; usernames cannot contain colon. No raw credential fields. |

Unknown fields and wrong types are errors throughout the config, even for
unselected actions. body_file, ca_file and env_file resolve from the TOML
folder independently of cwd; drive-relative Windows paths and NUL are rejected.
Only selected actions open body/credential/CA resources or invoke curl. list/plan
validate settings without opening those files, creating messages or resolving
clients. CA availability/format/trust errors are reported through the client.

MIME output uses encoded/folded UTF-8 subjects, quoted-printable plain-text bodies,
UTC dates, random message IDs and CRLF. Generated messages are bounded to 512 KiB;
client configuration is bounded to 64 KiB. Body files are size-checked before
allocation; detected truncation/growth/read errors fail the action, but concurrent
edits are not serialized.

## Transport, privacy and outcomes

TATR requires TLS and verifies the certificate and hostname. Private CA use is
supported without changing the OS trust store. Schannel may also require reachable
revocation information for private certificates; TATR does not bypass that check.
Default curl config files, ambient proxy settings, URL globbing and verbose traces
are disabled for SMTP. There is one attempt, no retry/spool/delivery daemon.
Any recipient rejection is a failure; failure can follow partial acceptance, and
manual retries can duplicate email.

Passwords travel in a private stdin pipe, not argv or generated temporary files.
The prepared message is written to an exclusively created private temporary
folder/file (POSIX 0700/0600; Windows protected current-user ACL), then removed
with checked cleanup. Filesystems that cannot provide private directory access
are rejected. TATR never inserts SMTP authentication into the message; explicit
user-supplied message contents remain user-controlled. Authentication also exists
in inherited child environments and process memory; privileged local processes
can access it. Interrupted/crashed runs can leave private temporary files behind.

SMTP client stdout/stderr are suppressed to the OS null device. TATR reports
controlled native/client statuses rather than attempting to redact arbitrary
server output. Ordinary exec children retain inherited I/O and can print secrets.
Task/hook names and diagnostic paths may disclose metadata.

SMTP actions use task/hook lifecycle logs and native curl exit codes. Successful
actions report that the client reported server acceptance; this is not proof of
recipient delivery. Common curl statuses include 1 (unsupported protocol),
6 (host lookup), 7 (connection), 28 (timeout), 55 (sending/recipient failure),
60 (certificate verification), 64 (required TLS unavailable), 67 (authentication)
and 77 (CA file). Context can affect the exact status; TATR retains it. Client
failure is TATR exit 1 for ordinary tasks; preparation/launch failures are exit 2.
Cleanup errors after successful sending are runner errors; an earlier sending
failure remains primary. A trusted custom client can return success without sending:
TATR relies on the chosen executable's contract.

No HTML, attachments, internationalized mailbox addresses, display names, OAuth,
retry, output capture, per-provider API, rotation, retention or release publication
is included. Existing interruption/process-tree limitations remain.

## Local verification

[examples/smtp.toml](../examples/smtp.toml) shows both action forms. Copy the
public [dotenv example](../examples/.env.smtp.example) to examples/.env.smtp and
configure your own server/addresses before sending. The portable demo deployment
uses CMake only as an example command; SMTP itself requires no CMake installation.
list/plan are safe for inspecting the example without credentials or a server.

Opt-in tests under tests/smtp-integration use only disposable loopback SMTP/TLS
servers, synthetic credentials and generated two-day certificates. They also serve
one local CRL endpoint on 127.0.0.1:18081 for Schannel; no system trust changes or
real recipient delivery occur. If that port is occupied, choose a coordinated
fixture certificate/endpoint change; do not stop an unrelated service.

```sh
podman build -t localhost/tatr-smtp-tests:alpine321 \
  -f tests/smtp-integration/Containerfile tests/smtp-integration
podman run --rm -v "$PWD:/src:rw" localhost/tatr-smtp-tests:alpine321 \
  python3 tests/smtp-integration/verify.py \
  --generate-certificates /src/build/smtp-test-certificates
podman run --rm --network none -v "$PWD:/src:rw" \
  localhost/tatr-smtp-tests:alpine321 \
  python3 tests/smtp-integration/verify.py \
  --tatr build/smtp-quality/tatr --child build/smtp-quality/tests/tatr_test_child \
  --curl /usr/bin/curl --certificates build/smtp-test-certificates \
  --output build/smtp-local-results
```

Use an Alpine-compatible test build for that image. On Windows run the same Python
script with native tatr.exe, test child and curl.exe paths, plus the generated
certificate directory. Output/certificate destinations must be fresh. Evidence
contains message fixtures and action logs but no authentication values.

Transport details follow the official [curl manual](https://curl.se/docs/manpage.html).
Message encoding follows [RFC 2045](https://www.rfc-editor.org/rfc/rfc2045) and
[RFC 2047](https://www.rfc-editor.org/rfc/rfc2047).
