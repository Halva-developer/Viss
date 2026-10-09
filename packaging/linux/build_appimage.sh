#!/usr/bin/env bash
# =============================================================================
# Viss AppImage Package Builder
# =============================================================================

set -e

VERSION="0.2.2"
ARCH="$(uname -m 2>/dev/null || echo x86_64)"
APP_DIR="/tmp/Viss.AppDir"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$SCRIPT_DIR"

echo "Building Viss AppImage..."

rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/usr/bin"
mkdir -p "$APP_DIR/usr/share/viss/libs"
mkdir -p "$APP_DIR/usr/include/viss"
mkdir -p dist

# 1. Compile binary
if [ ! -f "bin/viss" ]; then
    mkdir -p bin
    g++ -std=c++20 -O3 -pipe -I. -Ilibs -Isrc src/vissc.cpp -o bin/viss -pthread -ldl
fi

# 2. Populate AppDir
cp bin/viss "$APP_DIR/usr/bin/viss"
cp -r libs/* "$APP_DIR/usr/share/viss/libs/"
cp -r libs/* "$APP_DIR/usr/include/viss/"
cp packaging/linux/viss.desktop "$APP_DIR/"
if [ -f "extensions/viss-vscode/icon.png" ]; then
    cp extensions/viss-vscode/icon.png "$APP_DIR/viss.png"
fi

# 3. Create AppRun launcher
cat << 'EOF' > "$APP_DIR/AppRun"
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export PATH="${HERE}/usr/bin:${PATH}"
export VISS_STDLIB_DIR="${HERE}/usr/share/viss/libs"
exec "${HERE}/usr/bin/viss" "$@"
EOF
chmod 755 "$APP_DIR/AppRun"

# 4. Packaging with appimagetool if available
if command -v appimagetool >/dev/null 2>&1; then
    appimagetool "$APP_DIR" "dist/Viss-${VERSION}-${ARCH}.AppImage"
    echo "AppImage created: dist/Viss-${VERSION}-${ARCH}.AppImage ^_^"
else
    echo "Notice: 'appimagetool' not found in PATH."
    echo "AppDir staged cleanly at: $APP_DIR"
    echo "To build final AppImage on Linux, run: appimagetool $APP_DIR dist/Viss-${VERSION}-${ARCH}.AppImage"
fi
