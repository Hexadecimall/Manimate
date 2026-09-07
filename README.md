# Manimation

A visual editor for [Manim](https://www.manim.community). Design a scene on a
canvas and a timeline, and export Python you can run with `manim` on its own.

## Status

Early. The project model and the launcher are in place; the editor is next.

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
