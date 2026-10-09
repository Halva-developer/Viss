#!/usr/bin/env bash
# =============================================================================
# Viss Portable Linux Tarball Builder
# =============================================================================

set -e

VERSION="0.2.2"
ARCH="$(uname -m 2>/dev/null || echo x86_64)"
DIST_NAME="viss-${VERSION}-linux-${ARCH}"
STAGING_DIR="/tmp/${DIST_NAME}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$SCRIPT_DIR"

echo "Creating portable distribution tarball: ${DIST_NAME}.tar.gz..."

rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR/bin"
mkdir -p "$STAGING_DIR/libs"
mkdir -p dist

# 1. Ensure binary exists
if [ ! -f "bin/viss" ]; then
    mkdir -p bin
    g++ -std=c++20 -O3 -pipe -I. -Ilibs -Isrc src/vissc.cpp -o bin/viss -pthread -ldl
fi

# 2. Populate staging
cp bin/viss "$STAGING_DIR/bin/"
cp -r libs/* "$STAGING_DIR/libs/"
cp README.md "$STAGING_DIR/"
cp LICENSE "$STAGING_DIR/"
cp install.sh "$STAGING_DIR/"

# 3. Create tarball
tar -czf "dist/${DIST_NAME}.tar.gz" -C "/tmp" "$DIST_NAME"
echo "Distribution archive created: dist/${DIST_NAME}.tar.gz ^_^"

rm -rf "$STAGING_DIR"
