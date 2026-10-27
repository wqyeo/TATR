# Notification design

Run-completion hooks and structured SMTP actions are included in 0.2.0a.
The older 0.1.0a archives remain unchanged.

TATR owns task ordering, original-result selection and notification failure policy.
Structured SMTP configuration also prepares messages, removing email wrapper
scripts; an external curl executable provides SMTP/TLS transport. Ordinary
workflows do not require curl. Selection, rendering and process execution use
composition and small value types, without a general plugin framework.

- [Completion-hook contract](completion-hooks.md)
- [SMTP settings, privacy and delivery semantics](smtp.md)
- [Approved completion-hook increment](completion-hooks-increment.md)
- [Approved SMTP increment](smtp-increment.md)
- [Verification](verification.md)

Built-in SMTP/TLS, OAuth flows, attachments, retry/spooling and delivery guarantees
remain outside the implemented scope. Future increments require separate review.
