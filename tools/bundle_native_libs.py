#!/usr/bin/env python3
"""Make a Python runtime carry the native libraries its extensions need.

Manim draws through pycairo and manimpango. Neither publishes a wheel for
Linux, and pycairo publishes none for macOS either, so pip builds them from
source against whatever cairo and pango the build machine happens to have.
The result works there and nowhere else: the extension records an absolute
path to a library the user does not have.

This copies those libraries in beside the extensions and rewrites the paths so
each one is found relative to itself. Afterwards the runtime is self-contained
and can be moved to another machine.

Windows needs none of this: its wheels already ship their own DLLs.
"""

import argparse
import os
import pathlib
import shutil
import subprocess
import sys

# Libraries every machine already has, and which must NOT be copied: taking a
# private copy of the C library or the system frameworks is how you get a
# runtime that crashes in ways nobody can reproduce.
MACOS_SYSTEM_PREFIXES = ("/usr/lib/", "/System/")
LINUX_SYSTEM_NAMES = {
    "linux-vdso.so.1", "libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2",
    "librt.so.1", "libutil.so.1", "ld-linux-x86-64.so.2", "libgcc_s.so.1",
    "libstdc++.so.6", "libresolv.so.2",
}


def run(command):
    return subprocess.run(command, capture_output=True, text=True, check=False)


def extensions(root: pathlib.Path):
    """Every compiled extension in the runtime."""
    suffixes = (".so", ".dylib")
    return [p for p in root.rglob("*") if p.is_file() and p.suffix in suffixes]


def macos_dependencies(binary: pathlib.Path):
    result = run(["otool", "-L", str(binary)])
    found = []
    for line in result.stdout.splitlines()[1:]:
        path = line.strip().split(" ")[0]
        if not path or path.startswith("@"):
            continue
        if path.startswith(MACOS_SYSTEM_PREFIXES):
            continue
        found.append(path)
    return found


def linux_dependencies(binary: pathlib.Path):
    result = run(["ldd", str(binary)])
    found = []
    for line in result.stdout.splitlines():
        parts = line.split("=>")
        if len(parts) != 2:
            continue
        name = parts[0].strip()
        target = parts[1].strip().split(" ")[0]
        if not target.startswith("/") or name in LINUX_SYSTEM_NAMES:
            continue
        found.append(target)
    return found


def bundle(root: pathlib.Path, platform: str) -> int:
    library_dir = root / "lib" / "manimate-native"
    library_dir.mkdir(parents=True, exist_ok=True)

    dependencies = macos_dependencies if platform == "macos" else linux_dependencies

    # Follow dependencies of dependencies: cairo needs pixman, pango needs
    # glib, and none of those are on a clean machine either.
    pending = list(extensions(root))
    seen = set()
    copied = {}

    while pending:
        binary = pending.pop()
        if binary in seen:
            continue
        seen.add(binary)

        for source in dependencies(binary):
            name = os.path.basename(source)
            destination = library_dir / name
            if name not in copied:
                if not os.path.exists(source):
                    print(f"  missing, skipped: {source}")
                    continue
                shutil.copy2(source, destination, follow_symlinks=True)
                destination.chmod(destination.stat().st_mode | 0o755)
                copied[name] = destination
                pending.append(destination)
                print(f"  copied {name}")

    # Now point every binary at the copies rather than at the build machine.
    for binary in sorted(seen | set(copied.values())):
        if platform == "macos":
            relative = os.path.relpath(library_dir, binary.parent)
            for source in dependencies(binary):
                name = os.path.basename(source)
                if name not in copied:
                    continue
                run(["install_name_tool", "-change", source,
                     f"@loader_path/{relative}/{name}", str(binary)])
            run(["install_name_tool", "-id", f"@rpath/{binary.name}", str(binary)])
        else:
            relative = os.path.relpath(library_dir, binary.parent)
            run(["patchelf", "--set-rpath", f"$ORIGIN/{relative}", str(binary)])

    print(f"bundled {len(copied)} native libraries into {library_dir}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime", type=pathlib.Path, help="the Python runtime to make portable")
    parser.add_argument("--platform", choices=("macos", "linux"), required=True)
    arguments = parser.parse_args()

    if not arguments.runtime.is_dir():
        print(f"no such runtime: {arguments.runtime}", file=sys.stderr)
        return 1

    return bundle(arguments.runtime, arguments.platform)


if __name__ == "__main__":
    sys.exit(main())
