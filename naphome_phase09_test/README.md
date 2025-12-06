# Naphome Phase 0.9 Test Suite

This example implements a comprehensive test suite for validating all Phase 0.9 requirements from the Naphome specification.

## Overview

The test suite runs 12 tests covering all Phase 0.9 requirements:
1. ESP32-S3 System Initialization
2. SHT30 Temperature/Humidity Sensor
3. SGP30 VOC Sensor
4. BH1750 Light Sensor
5. SCD30 CO2 Sensor
6. PCA9685 RGB LED Control
7. WiFi Connectivity
8. AWS IoT Core MQTT Connectivity
9. ESP-SR Wake Word Detection
10. IR Blaster Functionality
11. Audio Output System (TPA3116D2)
12. Sensor Telemetry Publishing

## Features

- **LED Status Indicators**: Each test illuminates LEDs with colors indicating status:
  - **Green**: Test passed
  - **Yellow**: Test warning/partial implementation
  - **Red**: Test failed or not implemented

- **Google TTS Integration**: Uses Google Cloud Text-to-Speech API to speak test messages
  - Introduction: "This is a demo of the Naphome 0.9"
  - Each test announces its name and status
  - Final summary of test results

- **Comprehensive Testing**: Validates all hardware and software components specified in Phase 0.9

## Configuration

### Google TTS API Key

To enable Google TTS functionality, you need to:

1. Get a Google Cloud API key with Text-to-Speech API enabled:
   - Go to [Google Cloud Console](https://console.cloud.google.com/)
   - Create a new project or select an existing one
   - Enable the "Cloud Text-to-Speech API"
   - Create credentials (API Key)
   - Copy your API key

2. Set the API key in `main/naphome_test_suite.c`:
   ```c
   #define GOOGLE_TTS_API_KEY "YOUR_API_KEY_HERE"
   ```

**Note**: If the API key is not configured, the test suite will still run but will only log the text messages instead of speaking them. This allows testing without internet connectivity or API setup.

## Building and Flashing

```bash
cd naphome_phase09_test
idf.py build
idf.py flash monitor
```

## Test Execution

The test suite runs automatically on boot. It will:

1. Initialize the system
2. Speak the introduction message
3. Run each of the 12 tests sequentially
4. Display LED status for each test
5. Announce test results via TTS
6. Display final summary

## Test Status

- **Passed (Green)**: Feature is implemented and working correctly
- **Warning (Yellow)**: Feature is partially implemented or has issues
- **Failed/Not Implemented (Red)**: Feature is not yet implemented

## Hardware Requirements

- ESP32-S3-Korvo-1 v4.0 or compatible board
- LED strip (WS2812) on GPIO 19
- Audio output system
- WiFi connectivity (for Google TTS and cloud features)

## Notes

- Some tests may show "not yet implemented" status as features are developed
- Google TTS requires internet connectivity and API key
- LED colors may vary based on LED strip configuration (GRB format)

## Specification Reference

Full specification available at:
https://naptick.github.io/Naphome-Korvo1/phase-0.9-spec.html
