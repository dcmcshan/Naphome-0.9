# Naphome Phase 0.9 Implementation Plan

Based on TODO.md review and codebase analysis, this document outlines a structured plan to complete all Phase 0.9 requirements.

## Current Status Summary

- **Completed:** 3 tests (Tests 1, 6, 7) + 3 features
- **Partially Complete:** 3 tests (Tests 2, 3, 9, 11)
- **Not Started:** 6 tests (Tests 4, 5, 8, 10, 12)

---

## Phase 1: Quick Wins (High Priority, Low Effort)

**Goal:** Complete tests that have drivers but aren't integrated yet.

### Task 1.1: Integrate SHT30 Driver (Test 2)
**Status:** Driver exists, test stub returns `NOT_IMPLEMENTED`
**Estimated Time:** 30 minutes

**Steps:**
1. Add I2C bus initialization check in test function
2. Initialize SHT30 driver using existing `sht30_init()`
3. Read sensor data using `sht30_read()`
4. Validate data (check `valid` flag, reasonable ranges)
5. Set LED status based on hardware presence and data validity
6. Update TTS announcement with actual readings

**Acceptance Criteria:**
- Test reads actual sensor data (or synthetic if hardware absent)
- LED shows green if hardware present + valid data, yellow if synthetic, red if error
- TTS announces temperature and humidity values

---

### Task 1.2: Integrate SGP30 Driver (Test 3)
**Status:** Driver exists, test stub returns `NOT_IMPLEMENTED`
**Estimated Time:** 30 minutes

**Steps:**
1. Add I2C bus initialization check in test function
2. Initialize SGP30 driver using existing `sgp30_init()`
3. Read sensor data using `sgp30_read()`
4. Validate data (check `valid` flag, reasonable ranges)
5. Set LED status based on hardware presence and data validity
6. Update TTS announcement with actual readings

**Acceptance Criteria:**
- Test reads actual sensor data (or synthetic if hardware absent)
- LED shows green if hardware present + valid data, yellow if synthetic, red if error
- TTS announces VOC and eCO2 values

---

### Task 1.3: Update ESP-SR Test (Test 9)
**Status:** ESP-SR working, test stub returns `NOT_IMPLEMENTED`
**Estimated Time:** 20 minutes

**Steps:**
1. Check if `afe_handle` is initialized (global variable)
2. Check if `models` is loaded
3. Verify wake word detection is active
4. Set LED status based on initialization state
5. Update TTS announcement

**Acceptance Criteria:**
- Test verifies ESP-SR is initialized and ready
- LED shows green if initialized, red if not
- TTS confirms wake word detection status

---

## Phase 2: Sensor Drivers (Medium Priority, Medium Effort)

### Task 2.1: Implement BH1750 Light Sensor Driver (Test 4)
**Status:** Not implemented
**Estimated Time:** 2-3 hours

**Steps:**
1. Create `drivers/bh1750_driver.h`:
   - Define I2C address (0x23 or 0x5C)
   - Define command codes (power on, continuous measurement modes)
   - Define data structures (handle, data)
   - Function prototypes (init, deinit, read, is_hardware_present)

2. Create `drivers/bh1750_driver.c`:
   - Implement I2C communication
   - Add hardware detection with fallback to synthetic data
   - Implement measurement reading (16-bit lux value)
   - Add synthetic data generation (simulate day/night cycle)
   - Follow pattern from SHT30/SGP30 drivers

3. Integrate into test function:
   - Initialize driver
   - Read light level
   - Validate data
   - Set LED status
   - Update TTS announcement

**Acceptance Criteria:**
- Driver follows same pattern as SHT30/SGP30
- Supports hardware detection with synthetic fallback
- Test reads and validates light level
- LED and TTS feedback working

**Reference:** BH1750 datasheet, I2C address 0x23 (ADDR=LOW) or 0x5C (ADDR=HIGH)

---

### Task 2.2: Implement SCD30 CO2 Sensor Driver (Test 5)
**Status:** Not implemented
**Estimated Time:** 3-4 hours

**Steps:**
1. Create `drivers/scd30_driver.h`:
   - Define I2C address (0x61)
   - Define command codes (start measurement, read data, etc.)
   - Define data structures (handle, data with CO2, temp, humidity)
   - Function prototypes

2. Create `drivers/scd30_driver.c`:
   - Implement I2C communication (note: SCD30 uses CRC-8)
   - Add hardware detection with fallback to synthetic data
   - Implement measurement reading (CO2 ppm, temperature, humidity)
   - Add synthetic data generation
   - Follow pattern from SHT30/SGP30 drivers

3. Integrate into test function:
   - Initialize driver
   - Read CO2, temperature, humidity
   - Validate data
   - Set LED status
   - Update TTS announcement

**Acceptance Criteria:**
- Driver follows same pattern as other sensors
- Supports hardware detection with synthetic fallback
- Test reads and validates CO2, temperature, humidity
- LED and TTS feedback working

**Reference:** SCD30 datasheet, I2C address 0x61, uses CRC-8 checksum

---

## Phase 3: Audio System (Medium Priority, Medium Effort)

### Task 3.1: Implement Audio Output Test (Test 11)
**Status:** Audio initialized, test not implemented
**Estimated Time:** 2-3 hours

**Steps:**
1. Research ESP32-S3 audio playback APIs:
   - Check `esp_audio` API usage
   - Check I2S audio output functions
   - Verify TPA3116D2 amplifier control

2. Implement test function:
   - Generate simple tone/beep (sine wave or WAV file)
   - Play through audio system
   - Verify audio output (check for errors)
   - Set LED status
   - Update TTS announcement

3. Options for audio test:
   - Generate sine wave tone programmatically
   - Play a short WAV file from memory
   - Use ESP-Audio library if available

**Acceptance Criteria:**
- Test generates and plays audio
- LED shows green if audio plays, red if error
- TTS confirms audio test status

**Note:** May need to check `esp_board_init()` to see how audio is configured

---

## Phase 4: IR Blaster (Low-Medium Priority, Medium Effort)

### Task 4.1: Implement IR Blaster (Test 10)
**Status:** Not implemented
**Estimated Time:** 3-4 hours

**Steps:**
1. Identify IR transmitter GPIO pin:
   - Check hardware schematic
   - Check `esp_board_init()` or board config
   - Common pins: GPIO 4, 5, or dedicated IR LED pin

2. Create IR driver:
   - Use ESP32 RMT peripheral for IR transmission
   - Implement NEC protocol (most common)
   - Optionally support RC5, Sony SIRC
   - Create `drivers/ir_blaster.h` and `.c`

3. Implement test function:
   - Initialize RMT for IR transmission
   - Send test IR command (e.g., power on/off)
   - Verify transmission (check for errors)
   - Set LED status
   - Update TTS announcement

**Acceptance Criteria:**
- IR driver sends NEC protocol signals
- Test verifies transmission
- LED and TTS feedback working

**Reference:** ESP-IDF RMT driver documentation, NEC IR protocol spec

---

## Phase 5: Cloud Integration (Low Priority, High Effort)

### Task 5.1: AWS IoT Core MQTT Integration (Test 8)
**Status:** Not implemented
**Estimated Time:** 6-8 hours

**Steps:**
1. Add AWS IoT Core SDK or MQTT client:
   - Option A: Use `esp_mqtt` component (ESP-IDF)
   - Option B: Use AWS IoT Device SDK for Embedded C
   - Option C: Use `mqtt_client` component with custom AWS IoT connection

2. Certificate management:
   - Store device certificate, private key, CA certificate
   - Use NVS or filesystem for storage
   - Implement secure connection

3. Implement connection test:
   - Connect to AWS IoT Core endpoint
   - Subscribe to test topic
   - Publish test message
   - Verify connection
   - Set LED status
   - Update TTS announcement

**Acceptance Criteria:**
- Successfully connects to AWS IoT Core
- Can publish/subscribe to topics
- LED shows green if connected, red if error
- TTS confirms connection status

**Note:** Requires AWS IoT Core setup (thing, certificates, policies)

---

### Task 5.2: Sensor Telemetry Publishing (Test 12)
**Status:** Not implemented
**Estimated Time:** 4-6 hours

**Steps:**
1. Collect data from all sensors:
   - SHT30 (temperature, humidity)
   - SGP30 (VOC, eCO2)
   - BH1750 (light level)
   - SCD30 (CO2, temperature, humidity)

2. Format as JSON:
   - Create JSON structure with timestamp
   - Include all sensor readings
   - Add device ID/name

3. Publish via MQTT:
   - Use AWS IoT Core connection (from Test 8)
   - Publish to telemetry topic
   - Verify publication

4. Implement test function:
   - Collect all sensor data
   - Format JSON
   - Publish to MQTT
   - Verify success
   - Set LED status
   - Update TTS announcement

**Acceptance Criteria:**
- Collects data from all implemented sensors
- Formats valid JSON
- Publishes to AWS IoT Core successfully
- LED and TTS feedback working

**Dependencies:** Tests 2, 3, 4, 5, 8 must be complete

---

## Implementation Order Recommendation

### Sprint 1 (Quick Wins - 1-2 days):
1. ✅ Task 1.1: Integrate SHT30 (Test 2)
2. ✅ Task 1.2: Integrate SGP30 (Test 3)
3. ✅ Task 1.3: Update ESP-SR Test (Test 9)

### Sprint 2 (Sensor Drivers - 3-5 days):
4. ✅ Task 2.1: BH1750 Driver (Test 4)
5. ✅ Task 2.2: SCD30 Driver (Test 5)

### Sprint 3 (Audio & IR - 3-5 days):
6. ✅ Task 3.1: Audio Output Test (Test 11)
7. ✅ Task 4.1: IR Blaster (Test 10)

### Sprint 4 (Cloud Integration - 5-7 days):
8. ✅ Task 5.1: AWS IoT Core MQTT (Test 8)
9. ✅ Task 5.2: Sensor Telemetry (Test 12)

---

## Technical Notes

### I2C Bus Initialization
- Check if I2C bus is initialized before using sensor drivers
- May need to initialize I2C bus in `app_main()` if not already done
- Common I2C pins: SDA=GPIO21, SCL=GPIO22 (check board config)

### Driver Pattern
All sensor drivers should follow this pattern:
- `init()` - Initialize driver, detect hardware, set up synthetic data
- `deinit()` - Clean up resources
- `read()` - Read sensor data (hardware or synthetic)
- `is_hardware_present()` - Check if real hardware detected

### Test Function Pattern
All test functions should:
1. Announce test name via TTS
2. Initialize/check hardware
3. Perform test operation
4. Validate results
5. Set LED status (`led_set_status()`)
6. Announce results via TTS
7. Return `TEST_STATUS_PASS/WARNING/FAIL`

### LED Status Mapping
- `TEST_STATUS_PASS` → Green LED
- `TEST_STATUS_WARNING` → Yellow LED
- `TEST_STATUS_FAIL` → Red LED
- `TEST_STATUS_NOT_IMPLEMENTED` → Red LED

---

## Risk Assessment

**Low Risk:**
- Tasks 1.1, 1.2, 1.3 (integration tasks, drivers exist)
- Task 2.1 (BH1750 is simple I2C sensor)

**Medium Risk:**
- Task 2.2 (SCD30 has CRC requirements)
- Task 3.1 (audio APIs may need research)
- Task 4.1 (IR protocol timing critical)

**High Risk:**
- Task 5.1 (AWS IoT Core setup complexity)
- Task 5.2 (depends on multiple components)

---

## Success Metrics

- All 12 tests return `TEST_STATUS_PASS` or `TEST_STATUS_WARNING` (not `NOT_IMPLEMENTED`)
- All sensor drivers support hardware detection with synthetic fallback
- All tests provide LED and TTS feedback
- Code follows existing patterns and style
- No regressions in existing tests (Tests 1, 6, 7)

---

## Next Steps

1. Review this plan and adjust priorities if needed
2. Start with Sprint 1 (Quick Wins) for immediate progress
3. Test each implementation incrementally
4. Update TODO.md as tasks are completed
5. Document any deviations or discoveries
