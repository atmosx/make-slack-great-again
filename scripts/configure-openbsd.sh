#!/bin/sh
# Install what building msga needs on OpenBSD (Xenocara in base + packages),
# then check it.
#
# Usage:
#   scripts/configure-openbsd.sh
set -eu

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
ok()   { printf "  ${GREEN}ok${NC}    %s\n" "$*"; }
miss() { printf "  ${YELLOW}miss${NC}  %s\n" "$*"; }
die()  { printf "  ${RED}error${NC} %s\n" "$*" >&2; exit 1; }

[ "$(uname -s)" = "OpenBSD" ] || die "This script is for OpenBSD."

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [ -f /usr/X11R6/lib/pkgconfig/xcb.pc ] && [ -f /usr/X11R6/lib/pkgconfig/freetype2.pc ]; then
    ok "Xenocara (xcb + freetype2 in /usr/X11R6)"
else
    die "Xenocara (xbase/xshare/xfont/xserv) is missing from /usr/X11R6; install the X11 sets."
fi

# OpenBSD pkg_info stems / package names.
PACKAGES="
    bash
    cmake
    ninja
    git
    harfbuzz
    libxkbcommon
    dbus
"

to_install=""
for pkg in $PACKAGES; do
    if pkg_info -e "${pkg}-*" >/dev/null 2>&1; then
        ok "$pkg"
    else
        miss "$pkg"
        to_install="${to_install} ${pkg}"
    fi
done

# pkg-config comes with OpenBSD base; Python's package stem is "python",
# not "python3". Check the executables we actually need instead.
if command -v pkg-config >/dev/null 2>&1; then
    ok "pkg-config"
else
    die "pkg-config is missing from OpenBSD base; check your installed sets."
fi
if command -v python3 >/dev/null 2>&1; then
    ok "python3"
else
    miss "python3"
    to_install="${to_install} python"
fi

if [ -n "$to_install" ]; then
    echo ""
    echo "Installing:${to_install}"
    if [ "$(id -u)" -eq 0 ]; then
        pkg_add $to_install
    elif command -v doas >/dev/null 2>&1; then
        doas pkg_add $to_install
    else
        su root -c "pkg_add $to_install"
    fi
fi

for pkg in $PACKAGES; do
    pkg_info -e "${pkg}-*" >/dev/null 2>&1 || die "$pkg is still missing after pkg_add."
done
command -v python3 >/dev/null 2>&1 || die "python3 is still missing; install the Python 3 package."

if [ -d "${PROJECT_ROOT}/.git" ] || [ -f "${PROJECT_ROOT}/.git" ]; then
    git -C "$PROJECT_ROOT" config core.hooksPath .githooks
    ok "git hooks (.githooks/pre-commit)"
fi

echo ""
echo "Done. Build and test with:"
echo "  scripts/build.sh --test"
echo "Run with:"
echo "  ./build/msga"
