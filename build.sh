#!/bin/bash
# Build script for ZedLink Zed Extension
# Author: Krishna Teja Mekala

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZED_UNREAL_DIR="$SCRIPT_DIR/zed-unreal"

echo "==================================="
echo "Building ZedLink Zed Extension"
echo "==================================="

# Check for Rust
if ! command -v cargo &> /dev/null; then
    echo "Error: Rust/Cargo not found. Install from https://rustup.rs"
    exit 1
fi

# Build helper binary
echo ""
echo "[1/3] Building helper binary..."
cd "$ZED_UNREAL_DIR/zed-unreal-helper"
cargo build --release
echo "Helper binary built: $ZED_UNREAL_DIR/zed-unreal-helper/target/release/zed-unreal-helper"

# Install WASM target if needed
echo ""
echo "[2/3] Checking WASM target..."
if ! rustup target list --installed | grep -q "wasm32-wasip1"; then
    echo "Installing wasm32-wasip1 target..."
    rustup target add wasm32-wasip1
fi

# Build WASM extension
echo ""
echo "[3/3] Building WASM extension..."
cd "$ZED_UNREAL_DIR"
cargo build --release --target wasm32-wasip1

echo ""
echo "==================================="
echo "Build Complete!"
echo "==================================="
echo ""
echo "To install in Zed:"
echo "  1. Open Zed"
echo "  2. Cmd+Shift+P -> 'zed: install dev extension'"
echo "  3. Select: $ZED_UNREAL_DIR"
echo ""
