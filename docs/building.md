# Build and installation

Run these commands from the source checkout root.

Requirements: CMake 3.25 or newer, a compiler supporting C++23, and a C17 compiler
for the vendored TOML parser. On Windows,
install the Visual Studio C++ build tools. On Linux and macOS, use an appropriate
GCC or Clang toolchain and a supported CMake build tool.
POSIX builds require a libc with a child-directory spawn action; CMake checks
for the standard function or its `_np` variant.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

On Linux, CMake's default generator normally needs Make. If you installed Ninja
instead, select it on the first configure:

```sh
cmake --preset dev -G Ninja
cmake --build --preset dev
ctest --preset dev
```

Use the same generator when reusing a build directory. To change it, select a new
build directory or configure with `--fresh`. The release preset has its own build
directory, so select Ninja there too if needed: `cmake --preset release -G Ninja`.
These generator choices do not install the required build tool or compiler.

Run `build/dev/tatr` with a single-configuration generator, or
`build/dev/Debug/tatr.exe` with Visual Studio. The exact executable suffix and
configuration directory depend on the platform and generator.

For a release build optimized for size:

```sh
cmake --preset release
cmake --build --preset release
```

The release preset builds into `build/release`. Common executable locations are:

| Generator | Release executable |
| --- | --- |
| Linux single-configuration Ninja/Makefiles | `build/release/tatr` |
| Windows Visual Studio | `build/release/MinSizeRel/tatr.exe` |

To put the runnable distribution in a chosen directory, from the repository root:

```sh
cmake --install build/release --config MinSizeRel --prefix build/stage
```

With the default install directories, this produces:

```text
build/stage/
  bin/tatr                 # tatr.exe on Windows
  share/tatr/licenses/tatr-LICENSE
  share/tatr/licenses/tomlc17-LICENSE
```

Run the executable from `bin/`, or add that directory to PATH. The executable does
not need the source checkout at runtime. You can copy/move the installed directory
to another location on a compatible system, keeping both license notices when
redistributing it. TOML files, scripts, and programs named by your tasks still need
to be available. Linux binaries require compatible libc/C++ runtimes; a build for
one distribution is not automatically a portable Linux bundle.

For a first invocation from the repository root, use the installed executable
directly; no PATH change is needed:

```sh
# Linux (macOS is not yet verified)
./build/stage/bin/tatr --version
./build/stage/bin/tatr --file examples/tatr.toml run check -- --parallel 2
```

```powershell
# Windows PowerShell
.\build\stage\bin\tatr.exe --version
.\build\stage\bin\tatr.exe --file examples/tatr.toml run check -- --parallel 2
```

The example launches CMake/CTest by name, so those tools must be on PATH. If using
the repository's mise setup, prefix the invocation with `mise exec --`. For a
fresh Linux dev build with Ninja, set `CMAKE_GENERATOR=Ninja` for the invocation,
or configure `dev` with `-G Ninja` first. A preset's cached generator is reused.

Use installation or artifact copying for deployment. CMake build trees contain
cached source/build paths and should not be moved as a substitute for installation.

Optional: `mise run build`, `mise run test`, and `mise run release` wrap these commands. The
repository's `mise.toml` pins CMake; a C++ compiler must be installed separately.

For development checks, use `mise run strict`, `mise run quality`, and
`mise run sanitize`. Formatting and analysis tools are pinned in an optional
Podman image; see [the local workflow and CI proposal](development.md).

The TOML parser is vendored at a pinned revision; building a checkout requires no
dependency downloads. Installation includes the executable and both MIT notices.

To build and verify the local release archives, see [packaging instructions](packaging.md).

Windows debug/release and Linux x64 release builds have been verified. macOS is
unverified. See [verification and size measurements](verification.md) for
toolchains, commands, runtime dependencies, and limits of those measurements.

[Documentation index](README.md)
