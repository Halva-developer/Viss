#!/usr/bin/env bash
# =============================================================================
# Viss Language Universal Installer for Linux & macOS
# Version: 0.2.2 "Lemongrab & Lemonhope"
# =============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${CYAN}${BOLD}"
echo "  __      ___             "
echo "  \ \    / (_)            "
echo "   \ \  / / _ ___ ___     "
echo "    \ \/ / | / __/ __|    "
echo "     \  /  | \__ \__ \    "
echo "      \/   |_|___/___/    "
echo -e "${NC}"
echo -e "${GREEN}Viss Language Engine v0.2.2 \"Lemongrab & Lemonhope\" Installer${NC}"
echo -e "Target OS: $(uname -s) ($(uname -m))\n"

# 1. Check prerequisites
COMPILER=""
if command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
elif command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
else
    echo -e "${RED}[ERROR] Neither g++ nor clang++ was found!${NC}"
    echo "Please install a C++20 compiler first:"
    echo "  Ubuntu/Debian: sudo apt update && sudo apt install -y build-essential"
    echo "  Fedora:        sudo dnf install -y gcc-c++ make"
    echo "  Arch Linux:    sudo pacman -S base-devel gcc"
    echo "  macOS:         xcode-select --install"
    exit 1
fi

echo -e "${GREEN}[*] Found C++ compiler:${NC} $COMPILER ($($COMPILER --version | head -n 1))"

# 2. Determine installation location
INSTALL_PREFIX="/usr/local"
USE_SUDO=false

if [ "$EUID" -ne 0 ]; then
    if sudo -n true 2>/dev/null; then
        USE_SUDO=true
    elif [ -w "/usr/local/bin" ]; then
        USE_SUDO=false
    else
        INSTALL_PREFIX="$HOME/.local"
        USE_SUDO=false
    fi
fi

SUDO_CMD=""
if [ "$USE_SUDO" = true ]; then
    SUDO_CMD="sudo"
fi

BIN_DIR="$INSTALL_PREFIX/bin"
LIB_DIR="$INSTALL_PREFIX/share/viss/libs"
INC_DIR="$INSTALL_PREFIX/include/viss"

echo -e "${GREEN}[*] Target install directory:${NC} $BIN_DIR"
echo -e "${GREEN}[*] Standard library directory:${NC} $LIB_DIR"

# 3. Create build output
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

mkdir -p bin
echo -e "${CYAN}[*] Compiling Viss compiler with $COMPILER (-std=c++20 -O3)...${NC}"
$COMPILER -std=c++20 -O3 -pipe -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations \
    -I. -Ilibs -Isrc src/vissc.cpp -o bin/viss -pthread -ldl

echo -e "${GREEN}[*] Compiler built successfully!${NC}"

# 4. Install files
echo -e "${CYAN}[*] Installing binaries and standard libraries...${NC}"
$SUDO_CMD mkdir -p "$BIN_DIR"
$SUDO_CMD mkdir -p "$LIB_DIR"
$SUDO_CMD mkdir -p "$INC_DIR"

$SUDO_CMD cp bin/viss "$BIN_DIR/viss"
$SUDO_CMD chmod 755 "$BIN_DIR/viss"

$SUDO_CMD cp -r libs/* "$LIB_DIR/"
$SUDO_CMD cp -r libs/* "$INC_DIR/"

# 5. Linux Desktop Integration
if [ "$(uname -s)" = "Linux" ]; then
    MIME_DIR="$INSTALL_PREFIX/share/mime/packages"
    APP_DIR="$INSTALL_PREFIX/share/applications"
    ICON_DIR="$INSTALL_PREFIX/share/icons/hicolor/256x256/apps"

    if [ -d "packaging/linux" ]; then
        $SUDO_CMD mkdir -p "$MIME_DIR" "$APP_DIR" "$ICON_DIR"
        $SUDO_CMD cp packaging/linux/viss.xml "$MIME_DIR/" 2>/dev/null || true
        $SUDO_CMD cp packaging/linux/viss.desktop "$APP_DIR/" 2>/dev/null || true
        if [ -f "extensions/viss-vscode/icon.png" ]; then
            $SUDO_CMD cp extensions/viss-vscode/icon.png "$ICON_DIR/viss.png" 2>/dev/null || true
        fi
        command -v update-mime-database >/dev/null 2>&1 && $SUDO_CMD update-mime-database "$INSTALL_PREFIX/share/mime" || true
        command -v update-desktop-database >/dev/null 2>&1 && $SUDO_CMD update-desktop-database "$INSTALL_PREFIX/share/applications" || true
    fi
fi

# 6. Check PATH
case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *)
        echo -e "${YELLOW}[!] Notice: $BIN_DIR is not currently in your PATH.${NC}"
        echo -e "    Add it by appending this line to your ~/.bashrc or ~/.zshrc:"
        echo -e "    ${BOLD}export PATH=\"\$PATH:$BIN_DIR\"${NC}\n"
        ;;
esac

# 7. Verification
echo -e "\n${GREEN}${BOLD}=== Installation Complete! ^_^ ===${NC}"
"$BIN_DIR/viss" -v
echo -e "\nRun '${BOLD}viss --help${NC}' to get started!"
