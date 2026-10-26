# tomlc17

- Upstream: <https://github.com/cktan/tomlc17>
- Release: R261003
- Revision: `8d3766ddafa6515417f4885ae6878ef69fac401e`
- License: MIT; see `LICENSE`.
- Vendored files: upstream `src/tomlc17.c`, `src/tomlc17.h`, and `LICENSE`.
- Local modification: `page_create` uses `offsetof(page_t, data) + size`
  instead of computing an address through a null pointer. Linux Clang 19.1.4
  UBSan reported the upstream expression during ordinary configuration parsing.
  `<stddef.h>` supplies `offsetof`; allocation size and pool behavior are retained.

SHA-256 hashes of the upstream files:

```text
c382824bdfdd12f89a6a228d0838f99deae2f2c1763c3215547a0d7137ffa39d  tomlc17.c
fa7f05a6057d7b4da1b63ce6185f07f5c734c98043a5d3a30ee7b9289060cba8  tomlc17.h
f949d0976f85076c96fc5122f0632b5a1b806d8b2badbe968c42c97230e6e79e  LICENSE
```

SHA-256 of the locally patched `tomlc17.c`:

```text
1929a40192ff3b603ea56bc0695139d36a7ca8c3b781fef5c4147976669cc3ba  tomlc17.c
```

The C parser is compiled as a separate static target and accessed only from
`src/config.cpp`. The application is C++23. No downloads or package-manager setup
are needed to build a checkout. Installation includes the upstream license.

To update, review a specific upstream revision, check whether the local offset
fix is still needed, update this record, and run the normal and sanitizer tests.
