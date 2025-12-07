#!/bin/bash
# Test script to validate nap.local after flashing
# Usage: ./test_nap_local.sh

set -e

echo "=========================================="
echo "nap.local Validation Test"
echo "=========================================="
echo ""
echo "This script will:"
echo "1. Wait for device to boot and connect to WiFi"
echo "2. Wait for mDNS to be ready (nap.local)"
echo "3. Run Playwright tests to validate web server"
echo ""

# Check if playwright is installed
if ! command -v npx &> /dev/null; then
    echo "Error: npx not found. Please install Node.js and npm first."
    exit 1
fi

# Install dependencies if needed
if [ ! -d "node_modules" ]; then
    echo "Installing Playwright dependencies..."
    npm install
    npx playwright install chromium
fi

# Wait for device to be ready (check if nap.local resolves)
echo "Waiting for nap.local to be available..."
MAX_WAIT=60
WAIT_COUNT=0

while [ $WAIT_COUNT -lt $MAX_WAIT ]; do
    if ping -c 1 -W 1 nap.local &> /dev/null || curl -s --connect-timeout 2 http://nap.local &> /dev/null; then
        echo "✓ nap.local is accessible!"
        break
    fi
    echo "  Waiting... ($WAIT_COUNT/$MAX_WAIT seconds)"
    sleep 2
    WAIT_COUNT=$((WAIT_COUNT + 2))
done

if [ $WAIT_COUNT -ge $MAX_WAIT ]; then
    echo "⚠ Warning: nap.local not accessible after $MAX_WAIT seconds"
    echo "  The device may still be booting or WiFi may not be connected"
    echo "  Continuing with tests anyway..."
fi

echo ""
echo "Running Playwright tests..."
echo ""

# Run the tests
npx playwright test test_nap_local.js

echo ""
echo "=========================================="
echo "Tests completed!"
echo "=========================================="
