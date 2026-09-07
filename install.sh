#!/usr/bin/env sh
#
# Install Manimation on macOS or Linux.
#
#   curl -fsSL https://raw.githubusercontent.com/Hexadecimall/Manimation/main/install.sh | sh
#
# Options (pass after `-s --` when piping, e.g. `... | sh -s -- --channel stable`):
#   --channel dev|stable   Which builds to install. Default: dev.
#   --version <tag>        Install one exact release tag instead of the newest.
#   --prefix <dir>         Linux install prefix. Default: ~/.local
#   --list                 Show what would be installed and exit.
#
# POSIX sh: no bashisms, so it runs under dash on Debian and Ubuntu too.

set -eu

REPO="Hexadecimall/Manimation"
API="https://api.github.com/repos/$REPO/releases"

CHANNEL="dev"
VERSION=""
PREFIX="${HOME}/.local"
LIST_ONLY=0

# ------------------------------------------------------------------ output ---

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    BOLD=$(printf '\033[1m')
    DIM=$(printf '\033[2m')
    RED=$(printf '\033[31m')
    GREEN=$(printf '\033[32m')
    RESET=$(printf '\033[0m')
else
    BOLD='' DIM='' RED='' GREEN='' RESET=''
fi

say()  { printf '%s\n' "$*"; }
step() { printf '%s==>%s %s\n' "$BOLD" "$RESET" "$*"; }
note() { printf '    %s%s%s\n' "$DIM" "$*" "$RESET"; }
die()  { printf '%serror:%s %s\n' "$RED" "$RESET" "$*" >&2; exit 1; }

# ------------------------------------------------------------------- args ----

while [ $# -gt 0 ]; do
    case "$1" in
        --channel) CHANNEL="${2:-}"; shift 2 ;;
        --channel=*) CHANNEL="${1#*=}"; shift ;;
        --version) VERSION="${2:-}"; shift 2 ;;
        --version=*) VERSION="${1#*=}"; shift ;;
        --prefix) PREFIX="${2:-}"; shift 2 ;;
        --prefix=*) PREFIX="${1#*=}"; shift ;;
        --list) LIST_ONLY=1; shift ;;
        -h|--help) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

case "$CHANNEL" in
    dev|stable) ;;
    *) die "--channel must be dev or stable" ;;
esac

# ---------------------------------------------------------------- platform ---

need() { command -v "$1" >/dev/null 2>&1 || die "$1 is required but not installed"; }

need curl

case "$(uname -s)" in
    Darwin) PLATFORM=macos ;;
    Linux)  PLATFORM=linux ;;
    *) die "unsupported system: $(uname -s). Windows users want install.ps1." ;;
esac

case "$(uname -m)" in
    x86_64|amd64) ARCH=x86_64 ;;
    arm64|aarch64) ARCH=arm64 ;;
    *) die "unsupported architecture: $(uname -m)" ;;
esac

# --------------------------------------------------------------- discovery ---

# Pull the release list once and pick the newest asset built for this machine.
# Assets are named Manimation-<version>-<platform>-<arch>.<ext>, so matching on
# the asset name works whatever the tag happens to be called.
fetch_releases() {
    if [ -n "$VERSION" ]; then
        curl -fsSL -H 'Accept: application/vnd.github+json' "$API/tags/$VERSION" \
            || die "no release tagged $VERSION"
    else
        curl -fsSL -H 'Accept: application/vnd.github+json' "$API?per_page=60" \
            || die "could not reach GitHub"
    fi
}

# Extract download URLs from the JSON without depending on jq being installed.
asset_urls() {
    tr ',' '\n' \
        | grep '"browser_download_url"' \
        | sed 's/.*"browser_download_url": *"\([^"]*\)".*/\1/'
}

pick_asset() {
    # $1: newline-separated candidate URLs. Prefers the native installer.
    case "$PLATFORM" in
        macos) printf '%s\n' "$1" | grep -i '\.dmg$' | head -n 1 ;;
        linux)
            appimage=$(printf '%s\n' "$1" | grep -i '\.AppImage$' | head -n 1)
            if [ -n "$appimage" ]; then
                printf '%s\n' "$appimage"
            else
                printf '%s\n' "$1" | grep -i '\.tar\.gz$' | head -n 1
            fi
            ;;
    esac
}

step "Looking for a $PLATFORM $ARCH build"

RELEASES=$(fetch_releases)
# macOS ships one universal build that serves both architectures.
if [ "$PLATFORM" = macos ]; then
    MATCH="-macos-\\(${ARCH}\\|universal\\)\\."
else
    MATCH="-${PLATFORM}-${ARCH}\\."
fi

CANDIDATES=$(printf '%s' "$RELEASES" | asset_urls | grep -i -- "$MATCH" || true)

if [ "$CHANNEL" = stable ]; then
    CANDIDATES=$(printf '%s\n' "$CANDIDATES" | grep -v -- '-dev/' || true)
fi

[ -n "$CANDIDATES" ] || die "no $PLATFORM $ARCH build published yet"

URL=$(pick_asset "$CANDIDATES")
[ -n "$URL" ] || die "no installable asset for $PLATFORM $ARCH"

FILE=$(basename "$URL")
note "$FILE"

if [ "$LIST_ONLY" -eq 1 ]; then
    say "$URL"
    exit 0
fi

# ---------------------------------------------------------------- download ---

TMP=$(mktemp -d "${TMPDIR:-/tmp}/manimation.XXXXXX")
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT INT TERM

step "Downloading"
curl -fL --progress-bar -o "$TMP/$FILE" "$URL" || die "download failed"

# ----------------------------------------------------------------- install ---

install_macos() {
    step "Installing to /Applications"

    mountpoint="$TMP/mnt"
    mkdir -p "$mountpoint"
    hdiutil attach -nobrowse -quiet -mountpoint "$mountpoint" "$TMP/$FILE" \
        || die "could not open $FILE"
    # Always detach, even if the copy below fails.
    trap 'hdiutil detach "$mountpoint" -quiet >/dev/null 2>&1 || true; cleanup' EXIT INT TERM

    app=$(find "$mountpoint" -maxdepth 1 -name '*.app' | head -n 1)
    [ -n "$app" ] || die "no application found inside $FILE"

    target="/Applications"
    if [ ! -w "$target" ]; then
        target="$HOME/Applications"
        mkdir -p "$target"
        note "/Applications is not writable, using $target"
    fi

    name=$(basename "$app")
    rm -rf "${target:?}/${name:?}"
    cp -R "$app" "$target/" || die "could not copy into $target"

    # A downloaded bundle is quarantined; clear it so it opens without a detour
    # through System Settings.
    xattr -dr com.apple.quarantine "$target/$name" 2>/dev/null || true

    hdiutil detach "$mountpoint" -quiet >/dev/null 2>&1 || true
    trap cleanup EXIT INT TERM

    printf '%s✓%s Installed %s\n' "$GREEN" "$RESET" "$target/$name"
    note "Open it from Launchpad, or: open -a Manimation"
}

install_linux_appimage() {
    bindir="$PREFIX/bin"
    mkdir -p "$bindir"

    step "Installing to $bindir"
    install -m 0755 "$TMP/$FILE" "$bindir/Manimation" \
        || die "could not write to $bindir"

    # A desktop entry and icon, so it shows up in the application menu.
    appdir="$PREFIX/share/applications"
    icondir="$PREFIX/share/icons/hicolor/256x256/apps"
    mkdir -p "$appdir" "$icondir"

    cat > "$appdir/app.manimation.editor.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Manimation
GenericName=Manim Editor
Comment=Design Manim scenes on a canvas and a timeline
Exec=$bindir/Manimation %f
Icon=app.manimation.editor
Terminal=false
Categories=Graphics;AudioVideo;Video;
StartupWMClass=Manimation
DESKTOP

    # The AppImage carries its own icon; extract just that one file.
    (cd "$TMP" && "$bindir/Manimation" --appimage-extract '*.png' >/dev/null 2>&1) || true
    extracted=$(find "$TMP/squashfs-root" -maxdepth 2 -name '*.png' 2>/dev/null | head -n 1 || true)
    if [ -n "$extracted" ]; then
        cp "$extracted" "$icondir/app.manimation.editor.png"
    fi

    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$appdir" >/dev/null 2>&1 || true
    fi

    printf '%s✓%s Installed %s\n' "$GREEN" "$RESET" "$bindir/Manimation"

    case ":$PATH:" in
        *":$bindir:"*) ;;
        *) note "$bindir is not on your PATH; add it to run Manimation from a shell" ;;
    esac
}

install_linux_tarball() {
    libdir="$PREFIX/share/manimation"
    bindir="$PREFIX/bin"

    step "Installing to $libdir"
    rm -rf "$libdir"
    mkdir -p "$libdir" "$bindir"
    tar -xzf "$TMP/$FILE" -C "$libdir" --strip-components=1 \
        || die "could not extract $FILE"

    binary=$(find "$libdir" -type f -name Manimation -perm -u+x | head -n 1)
    [ -n "$binary" ] || die "no Manimation binary inside $FILE"

    ln -sf "$binary" "$bindir/Manimation"
    printf '%s✓%s Installed %s\n' "$GREEN" "$RESET" "$bindir/Manimation"
}

case "$PLATFORM" in
    macos) install_macos ;;
    linux)
        case "$FILE" in
            *.AppImage) install_linux_appimage ;;
            *) install_linux_tarball ;;
        esac
        ;;
esac
