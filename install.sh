#!/bin/bash
#
# PlumbrC Quick Install Script
#
# Usage: curl -sSL https://raw.githubusercontent.com/your-org/plumbrc/main/install.sh | bash
#
# Or: ./install.sh
#

set -e

INSTALL_DIR="/usr/local/bin"
PATTERN_DIR="/etc/plumbr/patterns"

echo "=== PlumbrC Installer ==="

# Check dependencies
if ! command -v gcc &> /dev/null; then
    echo "Error: gcc is required. Install with: apt install build-essential"
    exit 1
fi

if ! pkg-config --exists libpcre2-8 2>/dev/null; then
    echo "Error: libpcre2 is required. Install with: apt install libpcre2-dev"
    exit 1
fi

# Build
echo "Building PlumbrC..."
make clean
make -j$(nproc)

# Install
echo "Installing to $INSTALL_DIR..."
sudo install -m 755 build/bin/plumbr "$INSTALL_DIR/plumbr"

# Install patterns
echo "Installing patterns to $PATTERN_DIR..."
sudo mkdir -p "$PATTERN_DIR"
sudo cp -r patterns/* "$PATTERN_DIR/"

# Verify
echo ""
echo "=== Installation Complete ==="
plumbr --version
echo ""
echo "Usage examples:"
echo "  echo 'api_key=sk-secret123' | plumbr"
echo "  plumbr < app.log > safe.log"
echo "  tail -f /var/log/app.log | plumbr"
echo ""
echo "Pattern directory: $PATTERN_DIR"
