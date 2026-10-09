#!/usr/bin/env bash
# =============================================================================
# Viss Debian/Ubuntu (.deb) Package Builder
# =============================================================================

set -e

VERSION="0.2.2"
ARCH="$(dpkg --print-architecture 2>/dev/null || echo amd64)"
PACKAGE_NAME="viss_${VERSION}_${ARCH}"
BUILD_ROOT="/tmp/${PACKAGE_NAME}"

echo "Building Debian package: ${PACKAGE_NAME}.deb..."

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$SCRIPT_DIR"

# Clean prior staging
rm -rf "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT/DEBIAN"
mkdir -p "$BUILD_ROOT/usr/bin"
mkdir -p "$BUILD_ROOT/usr/share/viss/libs"
mkdir -p "$BUILD_ROOT/usr/include/viss"
mkdir -p "$BUILD_ROOT/usr/share/mime/packages"
mkdir -p "$BUILD_ROOT/usr/share/applications"
mkdir -p "$BUILD_ROOT/usr/share/icons/hicolor/256x256/apps"

# 1. Compile binary
if [ ! -f "bin/viss" ]; then
    echo "Compiling bin/viss..."
    mkdir -p bin
    g++ -std=c++20 -O3 -pipe -I. -Ilibs -Isrc src/vissc.cpp -o bin/viss -pthread -ldl
fi

# 2. Copy payload
cp bin/viss "$BUILD_ROOT/usr/bin/viss"
chmod 755 "$BUILD_ROOT/usr/bin/viss"

cp -r libs/* "$BUILD_ROOT/usr/share/viss/libs/"
cp -r libs/* "$BUILD_ROOT/usr/include/viss/"

cp packaging/linux/viss.xml "$BUILD_ROOT/usr/share/mime/packages/"
cp packaging/linux/viss.desktop "$BUILD_ROOT/usr/share/applications/"
if [ -f "extensions/viss-vscode/icon.png" ]; then
    cp extensions/viss-vscode/icon.png "$BUILD_ROOT/usr/share/icons/hicolor/256x256/apps/viss.png"
fi

# 3. Create DEBIAN/control
cat << 'EOF' > "$BUILD_ROOT/DEBIAN/control"
Package: viss
Version: 0.2.2
Section: devel
Priority: optional
Architecture: amd64
Maintainer: Halva <halva@horni.cc>
Depends: build-essential (>= 12.0) | g++ (>= 10.0) | clang (>= 12.0)
Description: Expressive and lightning-fast native systems scripting language
 Viss combines expressive syntax ergonomics with native C++20 machine speed.
 Features retro game engine, audio synthesizer, modern GUI, byte-level memory
 buffers, structured concurrency, and rich standard libraries.
EOF
sed -i "s/Architecture: amd64/Architecture: ${ARCH}/" "$BUILD_ROOT/DEBIAN/control"

# 4. Post-install and post-remove triggers
cat << 'EOF' > "$BUILD_ROOT/DEBIAN/postinst"
#!/bin/sh
set -e
if command -v update-mime-database >/dev/null 2>&1; then
    update-mime-database /usr/share/mime || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
exit 0
EOF
chmod 755 "$BUILD_ROOT/DEBIAN/postinst"

cat << 'EOF' > "$BUILD_ROOT/DEBIAN/postrm"
#!/bin/sh
set -e
if command -v update-mime-database >/dev/null 2>&1; then
    update-mime-database /usr/share/mime || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
exit 0
EOF
chmod 755 "$BUILD_ROOT/DEBIAN/postrm"

# 5. Build deb
mkdir -p dist
dpkg-deb --build --root-owner-group "$BUILD_ROOT" "dist/${PACKAGE_NAME}.deb"

echo "Package created successfully: dist/${PACKAGE_NAME}.deb ^_^"
rm -rf "$BUILD_ROOT"
