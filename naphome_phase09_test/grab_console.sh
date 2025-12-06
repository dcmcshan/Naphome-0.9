#!/bin/bash

# Script to grab 2 seconds of console output from ESP32-S3
# Usage: ./grab_console.sh [serial_port]

SERIAL_PORT="${1:-/dev/cu.usbserial-120}"
BAUD_RATE=115200

# Use Python for reliable timeout handling
python3 <<EOF
import serial
import time
import sys

try:
    ser = serial.Serial('$SERIAL_PORT', $BAUD_RATE, timeout=0.1)
    start = time.time()
    output = b''
    while time.time() - start < 2.0:
        if ser.in_waiting:
            output += ser.read(ser.in_waiting)
        time.sleep(0.01)
    ser.close()
    # Filter printable strings
    text = output.decode('utf-8', errors='ignore')
    for line in text.split('\n'):
        line = line.strip()
        if line and any(c.isprintable() for c in line):
            print(line)
except Exception as e:
    print(f"Error: {e}", file=sys.stderr)
    sys.exit(1)
EOF
