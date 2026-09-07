# Manimation

A visual editor for [Manim](https://www.manim.community). Design a scene on a
canvas and a timeline, and export Python you can run with `manim` on its own.

## Status

Early. The project model and the launcher are in place; the editor is next.

## Installing

Builds are published for macOS (Apple silicon and Intel), Linux and Windows.
Everything is bundled, so Qt does not need to be installed.

**macOS and Linux**

```sh
curl -fsSL https://raw.githubusercontent.com/Hexadecimall/Manimation/main/install.sh | sh
```

**Windows**

```powershell
irm https://raw.githubusercontent.com/Hexadecimall/Manimation/main/install.ps1 | iex
```

Both take `--channel stable` (`-Channel stable`) for released builds rather than
the newest one off `main`, and `--list` (`-List`) to print what they would
install without touching anything. On Windows, `-Portable` unpacks the zip
instead of running the installer, and needs no administrator rights.

Or take the package for your platform straight from
[Releases](https://github.com/Hexadecimall/Manimation/releases): a `.dmg` on
macOS, an `.AppImage` or tarball on Linux, an installer or portable `.zip` on
Windows.

## Building

Requires CMake 3.24+, a C++20 compiler and Qt 6.5+.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Layout

| Path       | Contents                                                        |
| ---------- | --------------------------------------------------------------- |
| `src/core` | Document model, `.manproj` format, project folders, recent list |
| `src/ui`   | Theme, custom window chrome, launcher                           |
| `src/app`  | Entry point                                                     |
| `tests`    | Test suite                                                      |

## A project on disk

```
My Project/
  My Project.manproj    the document; the only irreplaceable file
  Manimation/           derived state, safe to delete
  export/               standalone Python, runnable with `manim`
  output/               rendered video
  assets/               images, audio, anything the scene references
```
