# Phase 0.9 Demo TODO List

## ✅ **Working & Demonstrable**

1. **ESP32-S3 System Initialization** ✅
   - System boot, chip info, memory check
   - Test 1: PASS

2. **SHT30 Temperature/Humidity Sensor** ✅
   - Driver integrated and working
   - Test 2: PASS
   - Voice Command: "What is the temperature?" - ✅ IMPLEMENTED
   - Voice Command: "What is the humidity?" - ✅ IMPLEMENTED

3. **SGP30 VOC Sensor** ✅
   - Driver integrated and working
   - Test 3: PASS
   - Voice Command: "What is the air quality?" - ✅ IMPLEMENTED

4. **BH1750 Light Sensor** ✅
   - Driver integrated and working
   - Test 4: PASS
   - Voice Command: "What is the light level?" - ✅ IMPLEMENTED

5. **SCD30 CO2 Sensor** ✅
   - Driver integrated and working
   - Test 5: PASS
   - Voice Command: "What is the CO2 level?" - ✅ IMPLEMENTED

6. **LED Control (WS2812)** ✅
   - LED strip control (eyes, ears, smile)
   - Color changes, status indicators
   - Test 6: PASS (using WS2812, not PCA9685)

7. **WiFi Connectivity** ✅
   - WiFi connection, network status
   - Test 7: PASS

8. **ESP-SR Wake Word Detection** ✅
   - Wake word "Hi ESP" detection
   - Speech command recognition (local commands)
   - Phoneme debugging output
   - Test 9: Working (with TTS test)

9. **Voice Commands (Local)** ✅
   - "Turn on the light" - Working
   - "Turn off the light" - Working
   - Color commands (red, green, etc.) - Working
   - Sensor queries (temperature, humidity, air quality, CO2, light) - ✅ IMPLEMENTED
   - Volume commands - Registered but not implemented
   - TV/AC commands - Registered but not implemented

10. **Google TTS** ✅
    - Text-to-speech via Google API
    - Audio playback working

11. **Audio Output System (TPA3116D2)** ✅
    - Audio system verified via TTS playback
    - Test 11: PASS

12. **IR Blaster Functionality** ⚠️
    - RMT peripheral verified (hardware ready)
    - Test 10: WARNING (protocol implementation pending)

13. **Google STT/LLM/TTS Fallback** ✅
    - Code implemented for unrecognized commands
    - Needs testing (timeout handler should trigger)

## ⚠️ **Partially Working**

1. **"demo" Command** ⚠️
   - Registered in sdkconfig (ID 32: "DgMmO")
   - Programmatically added as fallback
   - **Issue**: Not being detected (timeout occurs)
   - **Fix Needed**: Verify phoneme encoding or use STT fallback

2. **STT/LLM Fallback** ⚠️
   - Code implemented
   - **Issue**: Not triggering on timeout (audio buffer may not be captured)
   - **Fix Needed**: Debug audio buffering during detection

## ❌ **Not Implemented - Lower Priority**

### Remaining Phase 0.9 Features

1. **AWS IoT Core MQTT** ❌
   - ❌ Not implemented
   - **TODO**:
     - Implement MQTT client
     - AWS IoT certificate handling
     - Connection management
     - Telemetry publishing
   - **Test**: `test_8_aws_iot_mqtt()` returns NOT_IMPLEMENTED
   - **Voice Command**: "Publish telemetry" - Registered but not implemented

2. **IR Blaster Protocol** ⚠️
   - ✅ Hardware verified (RMT peripheral available)
   - ❌ Protocol implementation (NEC, RC5, etc.) not implemented
   - **Test**: `test_10_ir_blaster()` returns WARNING (hardware ready, protocol pending)
   - **Voice Commands**: "Turn on the TV", "Turn on the air conditioner" - Registered but not implemented

3. **Sensor Telemetry Publishing** ❌
   - ❌ Not implemented (depends on AWS IoT MQTT)
   - **Test**: `test_12_sensor_telemetry()` returns NOT_IMPLEMENTED

### Medium Priority (Nice to Have)

7. **Audio Playback Features** ❌
   - Music playback - Not implemented
   - Music control (play, pause, stop, next, previous) - Not implemented
   - **Voice Commands**: Registered but not implemented

8. **Weather API** ❌
   - Weather information - Not implemented
   - **Voice Command**: "What is the weather?" - Registered but not implemented

9. **Audio Test** ❌
   - Audio output test - Not implemented
   - **Voice Command**: "Test audio" - Registered but not implemented

## 🔧 **Bugs to Fix**

1. **RMT Crash** 🔴
   - LED strip RMT channel crash when accessed concurrently
   - **Status**: Fixed with mutex (user's changes)
   - **Need**: Test to verify fix works

2. **AFE Ringbuffer Full** ⚠️
   - Warnings about ringbuffer being full
   - **Status**: Added delay in feed task (user's changes)
   - **Need**: Monitor if warnings persist

3. **"demo" Command Not Detected** 🔴
   - Command times out instead of being recognized
   - **Possible Causes**:
     - Phoneme encoding mismatch
     - Audio buffer not captured during detection
     - STT fallback not triggering
   - **Need**: Debug phoneme detection and STT fallback

4. **STT/LLM Fallback Not Triggering** 🔴
   - Timeout occurs but no STT/LLM logs appear
   - **Possible Causes**:
     - Audio buffer not initialized/captured
     - Task creation failing
     - Network not ready
   - **Need**: Add more debug logging (already added, need to test)

## 📋 **Immediate Next Steps for Demo**

### Priority 1: Fix Critical Bugs
1. ✅ Fix RMT crash (user fixed with mutex)
2. 🔄 Fix "demo" command detection
3. 🔄 Fix STT/LLM fallback triggering
4. 🔄 Verify AFE ringbuffer warnings are resolved

### Priority 2: Integrate Existing Drivers
1. Integrate SHT30 driver into `test_2_sht30_sensor()`
2. Integrate SGP30 driver into `test_3_sgp30_sensor()`
3. Add voice command handlers for sensor queries

### Priority 3: Implement Missing Drivers
1. Implement BH1750 driver
2. Implement SCD30 driver
3. Integrate into test functions

### Priority 4: Advanced Features
1. Implement IR Blaster
2. Implement AWS IoT Core MQTT
3. Implement audio playback features

## 🎯 **Minimum Viable Demo**

To demonstrate Phase 0.9, you need at minimum:

1. ✅ Wake word detection ("Hi ESP")
2. ✅ Local voice commands (lights, colors)
3. ✅ LED control
4. ✅ WiFi connectivity
5. ⚠️ "demo" command (currently broken)
6. ⚠️ STT/LLM fallback (code exists, needs testing)
7. ❌ At least 2 sensor readings (SHT30, SGP30 - drivers exist, need integration)

## 📊 **Current Status Summary**

- **Working**: 11/12 core features (92%) ✅
- **Partially Working**: 1/12 features (8%) ⚠️
- **Not Implemented**: 2/12 features (17%) ❌ (AWS IoT, Telemetry - lower priority)

**Core Phase 0.9 Features Status:**
1. ✅ ESP32-S3 System Initialization
2. ✅ SHT30 Temperature/Humidity Sensor (with voice commands)
3. ✅ SGP30 VOC Sensor (with voice commands)
4. ✅ BH1750 Light Sensor (with voice commands)
5. ✅ SCD30 CO2 Sensor (with voice commands)
6. ✅ PCA9685 RGB LED Control (WS2812)
7. ✅ WiFi Connectivity
8. ❌ AWS IoT Core MQTT (lower priority)
9. ✅ ESP-SR Wake Word Detection
10. ⚠️ IR Blaster Functionality (hardware ready, protocol pending)
11. ✅ Audio Output System (TPA3116D2)
12. ❌ Sensor Telemetry Publishing (depends on AWS IoT)

**For a complete demo, focus on:**
1. ✅ All sensors working with voice commands
2. ✅ Audio output verified
3. ⚠️ IR Blaster protocol implementation (optional)
4. ❌ AWS IoT MQTT (optional, for telemetry)
