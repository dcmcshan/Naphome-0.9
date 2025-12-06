# Naphome Phase 0.9 Implementation Plan

This document tracks the implementation plan for all Phase 0.9 features as specified in:
https://naptick.github.io/Naphome-Korvo1/phase-0.9-spec.html

## Test Suite Overview

The test suite consists of 12 tests covering all Phase 0.9 requirements. Each test validates hardware and software components.

---

## Current Status Summary

| Category | Complete | Partial | Not Started |
|----------|----------|---------|-------------|
| **Tests** | 3 | 4 | 5 |
| **Drivers** | 2 | 0 | 3 |
| **Features** | 3 | 0 | 0 |

### ✅ Completed (6 items)
- Test 1: ESP32-S3 System Initialization
- Test 6: PCA9685 RGB LED Control
- Test 7: WiFi Connectivity
- LED Face Pattern
- Google TTS Integration
- Voice Command "Run the Demo"

### ⚠️ Partially Complete (4 items)
- Test 2: SHT30 (driver exists, test not integrated)
- Test 3: SGP30 (driver exists, test not integrated)
- Test 9: ESP-SR (working, test function not implemented)
- Test 11: Audio Output (initialized, test not implemented)

### ❌ Not Started (5 items)
- Test 4: BH1750 Light Sensor
- Test 5: SCD30 CO2 Sensor
- Test 8: AWS IoT Core MQTT
- Test 10: IR Blaster
- Test 12: Sensor Telemetry Publishing

---

## Implementation Plan by Sprint

---

## Sprint 1: Quick Wins (High Priority, Low Effort)
**Goal:** Complete tests that have drivers but aren't integrated yet.  
**Estimated Time:** 1-2 days

### Task 1.1: Integrate SHT30 Driver (Test 2)
**Status:** ⚠️ DRIVER EXISTS, TEST NOT INTEGRATED  
**Priority:** HIGH  
**Estimated Time:** 30 minutes

**Current State:**
- ✅ Driver: `drivers/sht30_driver.c` - Complete
- ✅ Hardware detection with fallback to synthetic data
- ✅ I2C communication and CRC validation
- ❌ Test: `test_2_sht30_sensor()` - returns `TEST_STATUS_NOT_IMPLEMENTED`

**Implementation Steps:**
1. [ ] Add I2C bus initialization check in test function
2. [ ] Initialize SHT30 driver using `sht30_init()`
3. [ ] Read sensor data using `sht30_read()`
4. [ ] Validate data (check `valid` flag, reasonable ranges)
5. [ ] Set LED status using `led_set_status()` based on hardware presence and data validity
6. [ ] Update TTS announcement with actual temperature and humidity readings

**Acceptance Criteria:**
- Test reads actual sensor data (or synthetic if hardware absent)
- LED shows green if hardware present + valid data, yellow if synthetic, red if error
- TTS announces temperature and humidity values
- Returns `TEST_STATUS_PASS` or `TEST_STATUS_WARNING` (not `NOT_IMPLEMENTED`)

**Files to Modify:**
- `naphome_phase09_test/main/naphome_test_suite.c` - Update `test_2_sht30_sensor()`

---

### Task 1.2: Integrate SGP30 Driver (Test 3)
**Status:** ⚠️ DRIVER EXISTS, TEST NOT INTEGRATED  
**Priority:** HIGH  
**Estimated Time:** 30 minutes

**Current State:**
- ✅ Driver: `drivers/sgp30_driver.c` - Complete
- ✅ Hardware detection with fallback to synthetic data
- ✅ I2C communication, CRC validation, air quality initialization
- ❌ Test: `test_3_sgp30_sensor()` - returns `TEST_STATUS_NOT_IMPLEMENTED`

**Implementation Steps:**
1. [ ] Add I2C bus initialization check in test function
2. [ ] Initialize SGP30 driver using `sgp30_init()`
3. [ ] Read sensor data using `sgp30_read()`
4. [ ] Validate data (check `valid` flag, reasonable ranges)
5. [ ] Set LED status using `led_set_status()` based on hardware presence and data validity
6. [ ] Update TTS announcement with actual VOC and eCO2 readings

**Acceptance Criteria:**
- Test reads actual sensor data (or synthetic if hardware absent)
- LED shows green if hardware present + valid data, yellow if synthetic, red if error
- TTS announces VOC and eCO2 values
- Returns `TEST_STATUS_PASS` or `TEST_STATUS_WARNING` (not `NOT_IMPLEMENTED`)

**Files to Modify:**
- `naphome_phase09_test/main/naphome_test_suite.c` - Update `test_3_sgp30_sensor()`

---

### Task 1.3: Update ESP-SR Test (Test 9)
**Status:** ⚠️ PARTIALLY IMPLEMENTED  
**Priority:** HIGH  
**Estimated Time:** 20 minutes

**Current State:**
- ✅ ESP-SR initialization - Working
- ✅ Wake word model loading ("Hi ESP")
- ✅ Wake word detection working (confirmed in monitor logs)
- ✅ LED indication on wake word (ears illuminate)
- ✅ Voice command recognition ("run the demo")
- ✅ LED indication on command (smile)
- ❌ Test: `test_9_wake_word_detection()` - returns `TEST_STATUS_NOT_IMPLEMENTED`

**Implementation Steps:**
1. [ ] Check if `afe_handle` is initialized (global variable)
2. [ ] Check if `models` is loaded
3. [ ] Verify wake word detection is active
4. [ ] Set LED status using `led_set_status()` based on initialization state
5. [ ] Update TTS announcement with ESP-SR status

**Acceptance Criteria:**
- Test verifies ESP-SR is initialized and ready
- LED shows green if initialized, red if not
- TTS confirms wake word detection status
- Returns `TEST_STATUS_PASS` or `TEST_STATUS_FAIL` (not `NOT_IMPLEMENTED`)

**Files to Modify:**
- `naphome_phase09_test/main/naphome_test_suite.c` - Update `test_9_wake_word_detection()`

---

## Sprint 2: Sensor Drivers (Medium Priority, Medium Effort)
**Goal:** Implement missing sensor drivers and integrate them.  
**Estimated Time:** 3-5 days

### Task 2.1: Implement BH1750 Light Sensor Driver (Test 4)
**Status:** ❌ NOT IMPLEMENTED  
**Priority:** MEDIUM  
**Estimated Time:** 2-3 hours

**Current State:**
- ❌ Driver: Not created
- ❌ Test: `test_4_bh1750_sensor()` - returns `TEST_STATUS_NOT_IMPLEMENTED`
- ✅ Google TTS announcement stub exists

**Implementation Steps:**
1. [ ] Create `drivers/bh1750_driver.h`:
   - Define I2C address (0x23 or 0x5C)
   - Define command codes (power on, continuous measurement modes)
   - Define data structures (handle, data with lux value)
   - Function prototypes (init, deinit, read, is_hardware_present)

2. [ ] Create `drivers/bh1750_driver.c`:
   - Implement I2C communication
   - Add hardware detection with fallback to synthetic data
   - Implement measurement reading (16-bit lux value)
   - Add synthetic data generation (simulate day/night cycle)
   - Follow pattern from SHT30/SGP30 drivers

3. [ ] Integrate into test function:
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
- Returns `TEST_STATUS_PASS` or `TEST_STATUS_WARNING` (not `NOT_IMPLEMENTED`)

**Reference:** BH1750 datasheet, I2C address 0x23 (ADDR=LOW) or 0x5C (ADDR=HIGH)

**Files to Create:**
- `naphome_phase09_test/main/drivers/bh1750_driver.h`
- `naphome_phase09_test/main/drivers/bh1750_driver.c`

**Files to Modify:**
- `naphome_phase09_test/main/naphome_test_suite.c` - Update `test_4_bh1750_sensor()`
- `naphome_phase09_test/main/CMakeLists.txt` - Add driver source files

---

### Task 2.2: Implement SCD30 CO2 Sensor Driver (Test 5)
**Status:** ❌ NOT IMPLEMENTED  
**Priority:** MEDIUM  
**Estimated Time:** 3-4 hours

**Current State:**
- ❌ Driver: Not created
- ❌ Test: `test_5_scd30_sensor()` - returns `TEST_STATUS_NOT_IMPLEMENTED`
- ✅ Google TTS announcement stub exists

**Implementation Steps:**
1. [ ] Create `drivers/scd30_driver.h`:
   - Define I2C address (0x61)
   - Define command codes (start measurement, read data, etc.)
   - Define data structures (handle, data with CO2, temp, humidity)
   - Function prototypes

2. [ ] Create `drivers/scd30_driver.c`:
   - Implement I2C communication (note: SCD30 uses CRC-8)
   - Add hardware detection with fallback to synthetic data
   - Implement measurement reading (CO2 ppm, temperature, humidity)
   - Add synthetic data generation
   - Follow pattern from SHT30/SGP30 drivers

3. [ ] Integrate into test function:
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
- Returns `TEST_STATUS_PASS` or `TEST_STATUS_WARNING` (not `NOT_IMPLEMENTED`)

**Reference:** SCD30 datasheet, I2C address 0x61, uses CRC-8 checksum

**Files to Create:**
- `naphome_phase09_test/main/drivers/scd30_driver.h`
- `naphome_phase09_test/main/drivers/scd30_driver.c`

**Files to Modify:**
- `naphome_phase09_test/main/naphome_test_suite.c` - Update `test_5_scd30_sensor()`
- `naphome_phase09_test/main/CMakeLists.txt` - Add driver source files

---

## Sprint 3: Audio & IR (Medium Priority, Medium Effort)
**Goal:** Complete audio testing and implement IR blaster.  
**Estimated Time:** 3-5 days

### Task 3.1: Implement Audio Output Test (Test 11)
**Status:** ⚠️ PARTIALLY IMPLEMENTED  
**Priority:** MEDIUM  
**Estimated Time:** 2-3 hours

**Current State:**
- ✅ Audio system initialization (`esp_board_init()`) - Working
- ✅ Codec initialization (ES8311)
- ✅ I2S configuration
- ❌ Audio playback test
- ❌ TPA3116D2 amplifier control verification
- ❌ Test: `test_11_audio_output()` - returns `TEST_STATUS_NOT_IMPLEMENTED`

**Implementation Steps:**
1. [ ] Research ESP32-S3 audio playback APIs:
   - Check `esp_audio` API usage
   - Check I2S audio output functions
   - Verify TPA3116D2 amplifier control

2. [ ] Implement test function:
   - Generate simple tone/beep (sine wave or WAV file)
   - Play through audio system
   - Verify audio output (check for errors)
   - Set LED status
   - Update TTS announcement

3. [ ] Options for audio test:
   - Generate sine wave tone programmatically
   - Play a short WAV file from memory
   - Use ESP-Audio library if available

**Acceptance Criteria:**
- Test generates and plays audio
- LED shows green if audio plays, red if error
- TTS confirms audio test status
- Returns `TEST_STATUS_PASS` or `TEST_STATUS_FAIL` (not `NOT_IMPLEMENTED`)

**Note:** May need to check `esp_board_init()` to see how audio is configured

**Files to Modify:**
- `naphome_phase09_test/main/naphome_test_suite.c` - Update `test_11_audio_output()`

---

### Task 3.2: Implement IR Blaster (Test 10)
**Status:** ❌ NOT IMPLEMENTED  
**Priority:** MEDIUM  
**Estimated Time:** 3-4 hours

**Current State:**
- ❌ IR transmitter driver
- ❌ IR protocol support (NEC, RC5, etc.)
- ❌ Test: `test_10_ir_blaster()` - returns `TEST_STATUS_NOT_IMPLEMENTED`
- ✅ Google TTS announcement stub exists

**Implementation Steps:**
1. [ ] Identify IR transmitter GPIO pin:
   - Check hardware schematic
   - Check `esp_board_init()` or board config
   - Common pins: GPIO 4, 5, or dedicated IR LED pin

2. [ ] Create IR driver:
   - Use ESP32 RMT peripheral for IR transmission
   - Implement NEC protocol (most common)
   - Optionally support RC5, Sony SIRC
   - Create `drivers/ir_blaster.h` and `.c`

3. [ ] Implement test function:
   - Initialize RMT for IR transmission
   - Send test IR command (e.g., power on/off)
   - Verify transmission (check for errors)
   - Set LED status
   - Update TTS announcement

**Acceptance Criteria:**
- IR driver sends NEC protocol signals
- Test verifies transmission
- LED and TTS feedback working
- Returns `TEST_STATUS_PASS` or `TEST_STATUS_FAIL` (not `NOT_IMPLEMENTED`)

**Reference:** ESP-IDF RMT driver documentation, NEC IR protocol spec

**Files to Create:**
- `naphome_phase09_test/main/drivers/ir_blaster.h`
- `naphome_phase09_test/main/drivers/ir_blaster.c`

**Files to Modify:**
- `naphome_phase09_test/main/naphome_test_suite.c` - Update `test_10_ir_blaster()`
- `naphome_phase09_test/main/CMakeLists.txt` - Add driver source files

---

## Sprint 4: Cloud Integration (Low Priority, High Effort)
**Goal:** Implement AWS IoT Core MQTT and sensor telemetry publishing.  
**Estimated Time:** 5-7 days

### Task 4.1: AWS IoT Core MQTT Integration (Test 8)
**Status:** ❌ NOT IMPLEMENTED  
**Priority:** LOW  
**Estimated Time:** 6-8 hours

**Current State:**
- ❌ AWS IoT Core client implementation
- ❌ MQTT connection setup
- ❌ Certificate/credential management
- ❌ Test: `test_8_aws_iot_mqtt()` - returns `TEST_STATUS_NOT_IMPLEMENTED`
- ✅ Google TTS announcement stub exists

**Implementation Steps:**
1. [ ] Add AWS IoT Core SDK or MQTT client:
   - Option A: Use `esp_mqtt` component (ESP-IDF)
   - Option B: Use AWS IoT Device SDK for Embedded C
   - Option C: Use `mqtt_client` component with custom AWS IoT connection

2. [ ] Certificate management:
   - Store device certificate, private key, CA certificate
   - Use NVS or filesystem for storage
   - Implement secure connection

3. [ ] Implement connection test:
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
- Returns `TEST_STATUS_PASS` or `TEST_STATUS_FAIL` (not `NOT_IMPLEMENTED`)

**Note:** Requires AWS IoT Core setup (thing, certificates, policies)

**Files to Modify:**
- `naphome_phase09_test/main/naphome_test_suite.c` - Update `test_8_aws_iot_mqtt()`
- `naphome_phase09_test/main/idf_component.yml` - Add MQTT/AWS SDK dependency

---

### Task 4.2: Sensor Telemetry Publishing (Test 12)
**Status:** ❌ NOT IMPLEMENTED  
**Priority:** LOW  
**Estimated Time:** 4-6 hours

**Current State:**
- ❌ Telemetry data collection from all sensors
- ❌ Data formatting (JSON)
- ❌ Publishing mechanism (MQTT/HTTP)
- ❌ Test: `test_12_sensor_telemetry()` - returns `TEST_STATUS_NOT_IMPLEMENTED`
- ✅ Google TTS announcement stub exists

**Implementation Steps:**
1. [ ] Collect data from all sensors:
   - SHT30 (temperature, humidity)
   - SGP30 (VOC, eCO2)
   - BH1750 (light level)
   - SCD30 (CO2, temperature, humidity)

2. [ ] Format as JSON:
   - Create JSON structure with timestamp
   - Include all sensor readings
   - Add device ID/name

3. [ ] Publish via MQTT:
   - Use AWS IoT Core connection (from Test 8)
   - Publish to telemetry topic
   - Verify publication

4. [ ] Implement test function:
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
- Returns `TEST_STATUS_PASS` or `TEST_STATUS_FAIL` (not `NOT_IMPLEMENTED`)

**Dependencies:** Tests 2, 3, 4, 5, 8 must be complete

**Files to Modify:**
- `naphome_phase09_test/main/naphome_test_suite.c` - Update `test_12_sensor_telemetry()`

---

## Implementation Notes

### Driver Pattern
All sensor drivers should follow this pattern:
- `init()` - Initialize driver, detect hardware, set up synthetic data
- `deinit()` - Clean up resources
- `read()` - Read sensor data (hardware or synthetic)
- `is_hardware_present()` - Check if real hardware detected

### Test Function Pattern
All test functions should:
1. Announce test name via TTS (`speak_text()`)
2. Initialize/check hardware
3. Perform test operation
4. Validate results
5. Set LED status (`led_set_status()`)
6. Announce results via TTS
7. Return `TEST_STATUS_PASS/WARNING/FAIL` (not `NOT_IMPLEMENTED`)

### LED Status Mapping
- `TEST_STATUS_PASS` → Green LED
- `TEST_STATUS_WARNING` → Yellow LED
- `TEST_STATUS_FAIL` → Red LED
- `TEST_STATUS_NOT_IMPLEMENTED` → Red LED

### I2C Bus Initialization
- Check if I2C bus is initialized before using sensor drivers
- May need to initialize I2C bus in `app_main()` if not already done
- Common I2C pins: SDA=GPIO21, SCL=GPIO22 (check board config)

### General Notes
- All test functions have Google TTS announcement stubs
- LED status indication is implemented for all tests
- Synthetic data generation is implemented for SHT30 and SGP30 drivers
- ESP-SR is fully functional and working (confirmed in monitor logs)
- WiFi connectivity check works but requires credentials to be configured
- Partition table configured for 2MB factory app and 5MB model partition

---

## Success Metrics

- All 12 tests return `TEST_STATUS_PASS` or `TEST_STATUS_WARNING` (not `NOT_IMPLEMENTED`)
- All sensor drivers support hardware detection with synthetic fallback
- All tests provide LED and TTS feedback
- Code follows existing patterns and style
- No regressions in existing tests (Tests 1, 6, 7)

---

## Progress Tracking

**Sprint 1 Progress:** 0/3 tasks complete
- [ ] Task 1.1: Integrate SHT30 Driver
- [ ] Task 1.2: Integrate SGP30 Driver
- [ ] Task 1.3: Update ESP-SR Test

**Sprint 2 Progress:** 0/2 tasks complete
- [ ] Task 2.1: Implement BH1750 Driver
- [ ] Task 2.2: Implement SCD30 Driver

**Sprint 3 Progress:** 0/2 tasks complete
- [ ] Task 3.1: Implement Audio Output Test
- [ ] Task 3.2: Implement IR Blaster

**Sprint 4 Progress:** 0/2 tasks complete
- [ ] Task 4.1: AWS IoT Core MQTT Integration
- [ ] Task 4.2: Sensor Telemetry Publishing

**Overall Progress:** 0/9 tasks complete (0%)
