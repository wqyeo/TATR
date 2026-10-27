# Build destinations and file operations

Run repository example commands from the checkout root, with TATR and CMake on PATH.

TATR runs the commands you declare. Your build tool selects its build directory
and artifact names; an install or file-operation task selects the destination.
There are no built-in `build_dir`, `copy`, or `move` actions.

For a CMake project, save a workflow like this in the project's root:

```toml
version = 1

[tasks.configure]
exec = ["cmake", "-S", ".", "-B", "build/output", "-DCMAKE_BUILD_TYPE=Release"]
cwd = "."

[tasks.build]
depends = ["configure"]
exec = ["cmake", "--build", "build/output", "--config", "Release"]
cwd = "."

[tasks.install]
depends = ["build"]
exec = ["cmake", "--install", "build/output", "--config", "Release", "--prefix", "build/stage"]
cwd = "."
```

`tatr run install` builds in `build/output` and installs into `build/stage` according
to that project's CMake install rules. Artifact locations inside a build tree
depend on the project and generator; `--prefix` controls the install root.
Single-configuration generators use `CMAKE_BUILD_TYPE`; Visual Studio and other
multi-configuration generators select Release through `--config Release`.

Ordinary copying and moving also work through external commands. With CMake on
PATH, executable task bodies can use:

```toml
exec = ["cmake", "-E", "copy", "build/app.bin", "build/stage/app.bin"]
# Or, for a move on the same filesystem/volume:
# exec = ["cmake", "-E", "rename", "build/app.bin", "build/stage/app.bin"]
```

Create destination directories first with `cmake -E make_directory`; use
`copy_directory` for directory contents. CMake's `rename` supports files or
directories on one volume. For cross-volume moves, choose a tool with the required
semantics, or a script that copies, verifies success, then removes the source.
Linux/macOS can invoke `mv` directly; Windows PowerShell or cmd built-ins require
an explicit interpreter. The selected tool controls overwrite and error behavior.
See [CMake's command-line tools](https://cmake.org/cmake/help/v4.3/manual/cmake.1.html#run-a-command-line-tool).

Try the self-contained [file-operation example](../examples/file-operations.toml):

```sh
tatr --file examples/file-operations.toml plan move
tatr --file examples/file-operations.toml run move
```

It creates demo files only beneath `build/file-demo/`, copies the project license,
and moves the demo source into its output directory. CMake is a dependency of
these example tasks, not a requirement for every TATR workflow.

[Documentation index](README.md)
