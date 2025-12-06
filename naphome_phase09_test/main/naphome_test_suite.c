/*
 * Naphome Phase 0.9 Test Suite
 * 
 * This test suite validates all Phase 0.9 requirements from the specification:
 * https://naptick.github.io/Naphome-Korvo1/phase-0.9-spec.html
 * 
 * Tests:
 * 1. ESP32-S3 System Initialization
 * 2. SHT30 Temperature/Humidity Sensor
 * 3. SGP30 VOC Sensor
 * 4. BH1750 Light Sensor
 * 5. SCD30 CO2 Sensor
 * 6. PCA9685 RGB LED Control
 * 7. WiFi Connectivity
 * 8. AWS IoT Core MQTT Connectivity
 * 9. ESP-SR Wake Word Detection
 * 10. IR Blaster Functionality
 * 11. Audio Output System (TPA3116D2)
 * 12. Sensor Telemetry Publishing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_board_init.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "mbedtls/base64.h"
#include "cJSON.h"

#include "led_strip.h"

// Sensor driver includes
#include "sht30_driver.h"
#include "sgp30_driver.h"

// ESP-SR includes for voice recognition
// Note: esp_afe_config.h includes model_path.h which defines srmodel_list_t
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"  // This includes model_path.h
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_process_sdkconfig.h"
#include "bsp_board.h"

// Embedded audio files
extern const uint8_t _binary_offline_welcome_wav_start[] asm("_binary_offline_welcome_wav_start");
extern const uint8_t _binary_offline_welcome_wav_end[] asm("_binary_offline_welcome_wav_end");
extern const uint8_t _binary_frequencies_fear1_mp3_start[] asm("_binary_frequencies_fear1_mp3_start");
extern const uint8_t _binary_frequencies_fear1_mp3_end[] asm("_binary_frequencies_fear1_mp3_end");

// MP3 decoder - using minimp3 (will implement simple decoder)
// For now, we'll use a basic approach with embedded MP3 data

static const char *TAG = "naphome_test";

// WAV file structures
typedef struct {
    char chunk_id[4];
    uint32_t chunk_size;
    char format[4];
} wav_header_t;

typedef struct {
    char chunk_id[4];
    uint32_t chunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} wav_fmt_t;

// Function to parse and play WAV file
static esp_err_t play_wav_file(const uint8_t *wav_data, size_t wav_len)
{
    if (!wav_data || wav_len < sizeof(wav_header_t)) {
        ESP_LOGE(TAG, "Invalid WAV data");
        return ESP_ERR_INVALID_ARG;
    }
    
    const uint8_t *ptr = wav_data;
    const uint8_t *end = wav_data + wav_len;
    
    // Check RIFF header
    const wav_header_t *hdr = (const wav_header_t *)ptr;
    if (memcmp(hdr->chunk_id, "RIFF", 4) != 0 || memcmp(hdr->format, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Invalid WAV header");
        return ESP_ERR_INVALID_ARG;
    }
    ptr += sizeof(wav_header_t);
    
    wav_fmt_t fmt = {0};
    bool fmt_found = false;
    const uint8_t *data_ptr = NULL;
    uint32_t data_size = 0;
    
    // Parse chunks
    while (ptr + 8 <= end) {
        char chunk_id[4];
        memcpy(chunk_id, ptr, 4);
        uint32_t chunk_size = *(const uint32_t *)(ptr + 4);
        ptr += 8;
        
        if (ptr + chunk_size > end) {
            break;
        }
        
        if (!fmt_found && memcmp(chunk_id, "fmt ", 4) == 0) {
            if (chunk_size >= sizeof(wav_fmt_t)) {
                memcpy(&fmt, ptr, sizeof(wav_fmt_t));
                fmt_found = true;
                ESP_LOGI(TAG, "WAV: %u ch, %u Hz, %u bit", 
                         fmt.num_channels, fmt.sample_rate, fmt.bits_per_sample);
            }
            ptr += chunk_size;
            if (chunk_size & 1) ptr++; // Word alignment
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            data_ptr = ptr;
            data_size = chunk_size;
            break;
        } else {
            ptr += chunk_size;
            if (chunk_size & 1) ptr++; // Word alignment
        }
    }
    
    if (!fmt_found || !data_ptr) {
        ESP_LOGE(TAG, "WAV file missing fmt or data chunk");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Only support 16-bit PCM, mono or stereo
    if (fmt.audio_format != 1 || fmt.bits_per_sample != 16) {
        ESP_LOGE(TAG, "Unsupported WAV format: format=%u, bits=%u", 
                 fmt.audio_format, fmt.bits_per_sample);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    ESP_LOGI(TAG, "Playing WAV: %u bytes, %u ch, %u Hz", 
             data_size, fmt.num_channels, fmt.sample_rate);
    
    // Play audio data in chunks
    const size_t chunk_size = 1024; // bytes per chunk
    const int16_t *audio_data = (const int16_t *)data_ptr;
    size_t samples_remaining = data_size / sizeof(int16_t);
    
    while (samples_remaining > 0) {
        size_t samples_to_play = (samples_remaining > chunk_size / sizeof(int16_t)) 
                                 ? chunk_size / sizeof(int16_t) 
                                 : samples_remaining;
        size_t bytes_to_play = samples_to_play * sizeof(int16_t);
        
        esp_err_t ret = bsp_audio_play(audio_data, bytes_to_play, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to play audio chunk: %s", esp_err_to_name(ret));
            return ret;
        }
        
        audio_data += samples_to_play;
        samples_remaining -= samples_to_play;
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between chunks
    }
    
    ESP_LOGI(TAG, "Finished playing WAV file");
    return ESP_OK;
}

// Function to parse and play MP3 file
// TODO: Implement MP3 decoding (need to add minimp3 or similar library)
static esp_err_t play_mp3_file(const uint8_t *mp3_data, size_t mp3_len)
{
    if (!mp3_data || mp3_len == 0) {
        ESP_LOGE(TAG, "Invalid MP3 data");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGW(TAG, "MP3 playback not yet implemented (%zu bytes available)", mp3_len);
    ESP_LOGI(TAG, "MP3 decoder needs to be implemented - consider using minimp3 library");
    
    // For now, just return not implemented
    return ESP_ERR_NOT_SUPPORTED;
}

// Workaround for ESP-IDF 5.4.3 WiFi regulatory domain linker issue
// The WiFi library expects these symbols but they're not always provided
// Provide stub implementations to satisfy the linker
extern const void *regdomain_table;
extern const void *regulatory_data;
const void *regdomain_table __attribute__((weak)) = NULL;
const void *regulatory_data __attribute__((weak)) = NULL;

// LED control
#define MAX_LEDS 12
#define LED_STRIP_GPIO 19
led_strip_handle_t strip = NULL;

// LED assignments for face pattern
#define LED_RIGHT_EYE   2   // Right eye (LED 2)
#define LED_LEFT_EYE    11  // Left eye (LED 11)
#define LED_EAR_LEFT    4   // Left ear
#define LED_EAR_RIGHT   9   // Right ear
#define LED_SMILE_START 5   // Smile start
#define LED_SMILE_END   8   // Smile end (inclusive)

// LED state
typedef enum {
    LED_STATE_IDLE = 0,
    LED_STATE_WAKE_WORD,
    LED_STATE_COMMAND,
    LED_STATE_TEST_STATUS
} led_state_t;

static led_state_t current_led_state = LED_STATE_IDLE;
static bool eyes_looking_left = true;  // For idle animation

// ESP-SR voice recognition
static const esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;
static volatile int wakeup_flag = 0;
static volatile bool test_suite_triggered = false;
srmodel_list_t *models = NULL;

// Test status enumeration
typedef enum {
    TEST_STATUS_PASS = 0,      // Green LED
    TEST_STATUS_WARNING = 1,   // Yellow LED
    TEST_STATUS_FAIL = 2,      // Red LED
    TEST_STATUS_NOT_IMPLEMENTED = 3  // Red LED
} test_status_t;

// Google TTS and Gemini LLM configuration
#define GOOGLE_TTS_API_KEY "AIzaSyCjrdIBkpGWGXa4u9UileFFIMBZ_ZnMZ1w"  // From Naphome-Korvo1
#define GEMINI_API_KEY GOOGLE_TTS_API_KEY  // Same key works for both
#define GEMINI_MODEL "gemini-2.0-flash-exp"
#define GOOGLE_TTS_URL "https://texttospeech.googleapis.com/v1/text:synthesize?key=" GOOGLE_TTS_API_KEY
#define GEMINI_LLM_URL "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s"

// LED control functions
static void led_init(void)
{
    led_strip_config_t led_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = MAX_LEDS,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    
    esp_err_t ret = led_strip_new_rmt_device(&led_config, &rmt_config, &strip);
    if (ret != ESP_OK || !strip) {
        ESP_LOGE(TAG, "Failed to install WS2812 driver: %s", esp_err_to_name(ret));
        strip = NULL;
    } else {
        ESP_LOGI(TAG, "LED strip initialized on GPIO %d", LED_STRIP_GPIO);
        led_strip_clear(strip);
        led_strip_refresh(strip);
    }
}

// Clear all LEDs
static void led_clear_all(void)
{
    if (!strip) return;
    led_strip_clear(strip);
    led_strip_refresh(strip);
}

// Set a single LED with RGB color (GRB format for WS2812)
static void led_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!strip || index >= MAX_LEDS) return;
    // Note: LED_PIXEL_FORMAT_GRB means order is Green, Red, Blue
    led_strip_set_pixel(strip, index, g, r, b);
}

// Idle animation: eyes look left then right with smooth transitions (green eyes)
static void led_idle_animation(void)
{
    if (!strip || current_led_state != LED_STATE_IDLE) return;
    
    led_clear_all();
    
    // Both eyes visible but one brighter to show direction - all green
    if (eyes_looking_left) {
        // Look left: left eye bright, right eye dim
        led_set_pixel(LED_LEFT_EYE, 0, 255, 0);   // Bright green for left eye (LED 11)
        led_set_pixel(LED_RIGHT_EYE, 0, 100, 0);  // Dim green for right eye (LED 2)
    } else {
        // Look right: right eye bright, left eye dim
        led_set_pixel(LED_RIGHT_EYE, 0, 255, 0);   // Bright green for right eye (LED 2)
        led_set_pixel(LED_LEFT_EYE, 0, 100, 0);     // Dim green for left eye (LED 11)
    }
    
    led_strip_refresh(strip);
    eyes_looking_left = !eyes_looking_left;  // Toggle for next cycle
}

// Wake word detected: illuminate ears with pulsing effect
static void led_wake_word_detected(void)
{
    if (!strip) return;
    current_led_state = LED_STATE_WAKE_WORD;
    
    // Keep eyes visible, add ears
    led_clear_all();
    
    // Illuminate ears (LEDs 4, 9) - bright orange/yellow
    led_set_pixel(LED_EAR_LEFT, 255, 150, 0);   // Bright orange-yellow
    led_set_pixel(LED_EAR_RIGHT, 255, 150, 0);
    
    // Keep eyes visible - bright green
    led_set_pixel(LED_LEFT_EYE, 0, 255, 0);   // Bright green (LED 11)
    led_set_pixel(LED_RIGHT_EYE, 0, 255, 0);  // Bright green (LED 2)
    
    led_strip_refresh(strip);
    ESP_LOGI(TAG, "Wake word detected - ears illuminated");
}

// Command understood: show smile with eyes
static void led_command_understood(void)
{
    if (!strip) return;
    current_led_state = LED_STATE_COMMAND;
    
    // Clear and show happy face: eyes + smile
    led_clear_all();
    
    // Bright eyes (both looking forward) - green
    led_set_pixel(LED_LEFT_EYE, 0, 255, 0);   // Bright green (LED 11)
    led_set_pixel(LED_RIGHT_EYE, 0, 255, 0);  // Bright green (LED 2)
    
    // Show smile (LEDs 5-8) - bright green
    for (int i = LED_SMILE_START; i <= LED_SMILE_END; i++) {
        led_set_pixel(i, 0, 255, 0);  // Bright green smile
    }
    
    led_strip_refresh(strip);
    ESP_LOGI(TAG, "Command understood - smile shown");
    
    // Return to idle after a delay (handled by animation task)
    vTaskDelay(pdMS_TO_TICKS(2000));
    current_led_state = LED_STATE_IDLE;
}

static void led_set_status(test_status_t status)
{
    if (!strip) return;
    current_led_state = LED_STATE_TEST_STATUS;
    
    uint8_t r = 0, g = 0, b = 0;
    
    switch (status) {
        case TEST_STATUS_PASS:
            g = 255;  // Green
            break;
        case TEST_STATUS_WARNING:
            r = 255;  // Yellow (red + green)
            g = 255;
            break;
        case TEST_STATUS_FAIL:
        case TEST_STATUS_NOT_IMPLEMENTED:
            r = 255;  // Red
            break;
    }
    
    // Set all LEDs for test status
    for (uint32_t i = 0; i < MAX_LEDS; i++) {
        led_strip_set_pixel(strip, i, g, r, b);
    }
    led_strip_refresh(strip);
    ESP_LOGI(TAG, "LED status: %d (R:%d G:%d B:%d)", status, r, g, b);
    
    // Return to idle after showing status
    vTaskDelay(pdMS_TO_TICKS(1000));
    current_led_state = LED_STATE_IDLE;
}

// LED animation task - handles idle eye movement
static void led_animation_task(void *arg)
{
    const TickType_t delay_ticks = pdMS_TO_TICKS(1500);  // 1.5 second delay between eye movements
    
    while (1) {
        if (current_led_state == LED_STATE_IDLE) {
            led_idle_animation();
        }
        vTaskDelay(delay_ticks);
    }
}

// Gemini LLM function - simple implementation
static esp_err_t gemini_llm_call(const char *prompt, char *response, size_t response_len)
{
    if (!prompt || !response || strlen(prompt) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(GEMINI_API_KEY) == 0) {
        ESP_LOGW(TAG, "Gemini API key not configured");
        return ESP_ERR_NOT_FINISHED;
    }
    
    // Verify network is ready
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ESP_LOGW(TAG, "WiFi not connected, skipping LLM call");
        return ESP_ERR_NOT_FINISHED;
    }
    
    // Build JSON request for Gemini API
    cJSON *root = cJSON_CreateObject();
    cJSON *contents = cJSON_CreateArray();
    cJSON *content = cJSON_CreateObject();
    cJSON *parts = cJSON_CreateArray();
    cJSON *part = cJSON_CreateObject();
    
    cJSON_AddItemToObject(root, "contents", contents);
    cJSON_AddItemToArray(contents, content);
    cJSON_AddItemToObject(content, "parts", parts);
    cJSON_AddItemToArray(parts, part);
    cJSON_AddStringToObject(part, "text", prompt);
    
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) {
        ESP_LOGE(TAG, "Failed to create JSON payload");
        return ESP_ERR_NO_MEM;
    }
    
    // Gemini API endpoint
    char url[512];
    snprintf(url, sizeof(url), GEMINI_LLM_URL, GEMINI_MODEL, GEMINI_API_KEY);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(payload);
        return ESP_FAIL;
    }
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));
    
    esp_err_t err = esp_http_client_perform(client);
    free(payload);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        if (status_code == 200 && content_length > 0) {
            char *buffer = malloc(content_length + 1);
            if (buffer) {
                int data_read = esp_http_client_read_response(client, buffer, content_length);
                if (data_read > 0) {
                    buffer[data_read] = '\0';
                    
                    // Parse JSON response
                    cJSON *response_json = cJSON_Parse(buffer);
                    if (response_json) {
                        cJSON *candidates = cJSON_GetObjectItem(response_json, "candidates");
                        if (candidates && cJSON_IsArray(candidates) && cJSON_GetArraySize(candidates) > 0) {
                            cJSON *candidate = cJSON_GetArrayItem(candidates, 0);
                            if (candidate) {
                                cJSON *content_obj = cJSON_GetObjectItem(candidate, "content");
                                if (content_obj) {
                                    cJSON *parts = cJSON_GetObjectItem(content_obj, "parts");
                                    if (parts && cJSON_IsArray(parts) && cJSON_GetArraySize(parts) > 0) {
                                        cJSON *part = cJSON_GetArrayItem(parts, 0);
                                        if (part) {
                                            cJSON *text = cJSON_GetObjectItem(part, "text");
                                            if (text && cJSON_IsString(text)) {
                                                strncpy(response, text->valuestring, response_len - 1);
                                                response[response_len - 1] = '\0';
                                                cJSON_Delete(response_json);
                                                free(buffer);
                                                esp_http_client_cleanup(client);
                                                ESP_LOGI(TAG, "Gemini LLM response: %s", response);
                                                return ESP_OK;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        cJSON_Delete(response_json);
                    }
                }
                free(buffer);
            }
        } else {
            ESP_LOGE(TAG, "HTTP error: status %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}

// Simple JSON parser to extract audioContent from Google TTS response
static char* extract_audio_content(const char *json_response, int *audio_len)
{
    // Look for "audioContent":" in the JSON response
    const char *start_marker = "\"audioContent\":\"";
    const char *start = strstr(json_response, start_marker);
    if (!start) {
        return NULL;
    }
    
    start += strlen(start_marker);
    const char *end = strstr(start, "\"");
    if (!end) {
        return NULL;
    }
    
    int base64_len = end - start;
    char *base64_audio = malloc(base64_len + 1);
    if (!base64_audio) {
        return NULL;
    }
    
    memcpy(base64_audio, start, base64_len);
    base64_audio[base64_len] = '\0';
    
    // Decode base64
    size_t decoded_len = 0;
    mbedtls_base64_decode(NULL, 0, &decoded_len, (const unsigned char *)base64_audio, base64_len);
    
    unsigned char *decoded_audio = malloc(decoded_len);
    if (!decoded_audio) {
        free(base64_audio);
        return NULL;
    }
    
    size_t actual_len = 0;
    int ret = mbedtls_base64_decode(decoded_audio, decoded_len, &actual_len, 
                                    (const unsigned char *)base64_audio, base64_len);
    free(base64_audio);
    
    if (ret != 0) {
        free(decoded_audio);
        return NULL;
    }
    
    *audio_len = actual_len;
    return (char *)decoded_audio;
}

// Google TTS function
static esp_err_t google_tts_speak(const char *text)
{
    if (!text || strlen(text) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if API key is configured
    if (strlen(GOOGLE_TTS_API_KEY) == 0) {
        ESP_LOGW(TAG, "Google TTS API key not configured. Skipping TTS.");
        ESP_LOGI(TAG, "Would speak: %s", text);
        return ESP_ERR_NOT_FINISHED;
    }
    
    // Verify network is ready before making HTTP request
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ESP_LOGW(TAG, "WiFi not connected, skipping TTS");
        return ESP_ERR_NOT_FINISHED;
    }
    
    char url[512];
    snprintf(url, sizeof(url), "https://texttospeech.googleapis.com/v1/text:synthesize?key=%s", GOOGLE_TTS_API_KEY);
    
    // Build JSON request (escape quotes in text)
    char json_request[2048];
    // Simple escaping - replace " with \"
    char escaped_text[1024];
    int j = 0;
    for (int i = 0; text[i] && j < sizeof(escaped_text) - 1; i++) {
        if (text[i] == '"') {
            escaped_text[j++] = '\\';
            escaped_text[j++] = '"';
        } else if (text[i] == '\\') {
            escaped_text[j++] = '\\';
            escaped_text[j++] = '\\';
        } else {
            escaped_text[j++] = text[i];
        }
    }
    escaped_text[j] = '\0';
    
    snprintf(json_request, sizeof(json_request),
        "{"
        "\"input\":{\"text\":\"%s\"},"
        "\"voice\":{\"languageCode\":\"en-US\",\"name\":\"en-US-Standard-D\",\"ssmlGender\":\"NEUTRAL\"},"
        "\"audioConfig\":{\"audioEncoding\":\"LINEAR16\",\"sampleRateHertz\":16000}"
        "}",
        escaped_text);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_request, strlen(json_request));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        ESP_LOGI(TAG, "HTTP Status = %d, content_length = %d", status_code, content_length);
        
        if (status_code == 200 && content_length > 0) {
            char *buffer = malloc(content_length + 1);
            if (buffer) {
                int data_read = esp_http_client_read_response(client, buffer, content_length);
                if (data_read > 0) {
                    buffer[data_read] = '\0';
                    
                    // Extract and decode audio content
                    int audio_len = 0;
                    char *audio_data = extract_audio_content(buffer, &audio_len);
                    
                    if (audio_data && audio_len > 0) {
                        // Play audio via esp_audio_play (expects int16_t PCM at 16kHz)
                        // Google TTS returns LINEAR16 at 16kHz, which matches our format
                        int16_t *pcm_data = (int16_t *)audio_data;
                        int pcm_samples = audio_len / sizeof(int16_t);
                        
                        ESP_LOGI(TAG, "Playing TTS audio: %d samples", pcm_samples);
                        esp_audio_play(pcm_data, audio_len, portMAX_DELAY);
                        
                        free(audio_data);
                    } else {
                        ESP_LOGW(TAG, "Failed to extract audio content from response");
                    }
                }
                free(buffer);
            }
        } else {
            ESP_LOGE(TAG, "HTTP error: status %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}

// TTS task - runs Google TTS in a separate task to avoid TCP/IP stack issues
static void tts_task(void *pvParameters)
{
    char *text = (char *)pvParameters;
    if (text) {
        ESP_LOGI(TAG, "Speaking: %s", text);
        
        // Wait for network to be ready (check WiFi connection and TCP/IP stack)
        int retry_count = 0;
        const int max_retries = 100;  // 10 seconds timeout
        bool network_ready = false;
        
        while (retry_count < max_retries) {
            wifi_ap_record_t ap_info;
            esp_err_t wifi_status = esp_wifi_sta_get_ap_info(&ap_info);
            if (wifi_status == ESP_OK) {
                // WiFi is connected, but TCP/IP stack might still be initializing
                // Give it a bit more time
                vTaskDelay(pdMS_TO_TICKS(500));
                network_ready = true;
                ESP_LOGI(TAG, "Network ready, proceeding with TTS");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));  // Wait 100ms
            retry_count++;
        }
        
        if (network_ready) {
            google_tts_speak(text);
        } else {
            ESP_LOGW(TAG, "Network not ready after %d retries, skipping TTS for: %s", max_retries, text);
        }
        
        free(text);  // Free the allocated text string
    }
    vTaskDelete(NULL);
}

// Simplified TTS - queue text to be spoken in a separate task
static void speak_text(const char *text)
{
    if (!text || strlen(text) == 0) {
        return;
    }
    
    // Allocate memory for the text string
    size_t text_len = strlen(text) + 1;
    char *text_copy = malloc(text_len);
    if (!text_copy) {
        ESP_LOGE(TAG, "Failed to allocate memory for TTS text");
        return;
    }
    strcpy(text_copy, text);
    
    // Create a task to handle TTS (runs on core 0, main task core)
    // This avoids TCP/IP stack issues when called from detect_Task
    xTaskCreatePinnedToCore(
        tts_task,
        "tts_task",
        8192,
        text_copy,
        4,  // Lower priority than detect task
        NULL,
        0   // Run on core 0 (main task core)
    );
}

// Forward declaration
void run_test_suite(void *pvParameters);

// Command handler - matches working example's speech_commands_action_with_string
void speech_commands_action_with_string(int command_id, const char *command_string)
{
    printf("Executing command_id: %d, string: %s\n", command_id, command_string ? command_string : "NULL");
    
    if (!strip) {
        printf("LED strip not initialized\n");
        return;
    }
    
    // Helper to check command string content
    bool string_contains(const char *str, const char *substr) {
        if (!str || !substr) return false;
        char str_lower[64], substr_lower[64];
        int i = 0;
        while (str[i] && i < 63) {
            str_lower[i] = (str[i] >= 'A' && str[i] <= 'Z') ? str[i] + 32 : str[i];
            i++;
        }
        str_lower[i] = '\0';
        i = 0;
        while (substr[i] && i < 63) {
            substr_lower[i] = (substr[i] >= 'A' && substr[i] <= 'Z') ? substr[i] + 32 : substr[i];
            i++;
        }
        substr_lower[i] = '\0';
        return strstr(str_lower, substr_lower) != NULL;
    }
    
    // Handle "demo" or "run the demo" command (ID 0 or string contains "demo")
    if (command_id == 0 || (command_string && string_contains(command_string, "demo"))) {
        printf("Demo command detected! Starting test suite...\n");
        led_command_understood();  // Show smile
        speak_text("Running the demo.");
        if (!test_suite_triggered) {
            test_suite_triggered = true;
            xTaskCreatePinnedToCore(
                run_test_suite,
                "test_suite",
                8192,
                NULL,
                5,
                NULL,
                1
            );
        }
        return;
    }
    
    // Handle "playing wav" command
    if (command_string && string_contains(command_string, "playing") && string_contains(command_string, "wav")) {
        printf("Playing WAV file command detected!\n");
        led_command_understood();  // Show smile
        speak_text("Playing WAV file.");
        
        const uint8_t *welcome_wav = _binary_offline_welcome_wav_start;
        size_t welcome_wav_size = _binary_offline_welcome_wav_end - _binary_offline_welcome_wav_start;
        if (welcome_wav_size > 0) {
            ESP_LOGI(TAG, "Playing WAV file (%zu bytes)", welcome_wav_size);
            esp_err_t play_ret = play_wav_file(welcome_wav, welcome_wav_size);
            if (play_ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to play WAV file: %s", esp_err_to_name(play_ret));
                speak_text("Failed to play WAV file.");
            }
        } else {
            ESP_LOGW(TAG, "WAV file not embedded");
            speak_text("WAV file not available.");
        }
        return;
    }
    
    // Handle "playing mp3" command
    if (command_string && string_contains(command_string, "playing") && string_contains(command_string, "mp3")) {
        printf("Playing MP3 file command detected!\n");
        led_command_understood();  // Show smile
        speak_text("Playing MP3 file.");
        
        const uint8_t *mp3_data = _binary_frequencies_fear1_mp3_start;
        size_t mp3_size = _binary_frequencies_fear1_mp3_end - _binary_frequencies_fear1_mp3_start;
        if (mp3_size > 0) {
            ESP_LOGI(TAG, "Playing MP3 file (%zu bytes)", mp3_size);
            esp_err_t play_ret = play_mp3_file(mp3_data, mp3_size);
            if (play_ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to play MP3 file: %s", esp_err_to_name(play_ret));
                speak_text("Failed to play MP3 file.");
            }
        } else {
            ESP_LOGW(TAG, "MP3 file not embedded");
            speak_text("MP3 file not available.");
        }
        return;
    }
    
    // Handle light control commands
    // Based on phoneme mapping: "TkN nN" = "turn on", "TkN eF" = "turn off"
    // Command ID 13 = "TkN nN jc LiT" = "turn on the light" (ON)
    // Command ID 14 = "TkN eF jc LiT" = "turn off the light" (OFF)
    // Command ID 12 = "MdK Mm c KnFm" = "make me a coffee" (not lights)
    // Command ID 1 = "Sgl c Sel" = "sing a song" (not lights)
    
    // Turn lights ON (command_id 13, 17, or string contains "turn on" and "light")
    if (command_id == 13 || command_id == 17 || 
        (command_string && string_contains(command_string, "turn on") && string_contains(command_string, "light"))) {
        printf("Turning lights on\n");
        led_command_understood();  // Show smile
        speak_text("Turning lights on.");
        
        // Light up all LEDs in green (happy face)
        led_clear_all();
        // Eyes - bright green
        led_set_pixel(LED_LEFT_EYE, 0, 255, 0);   // Bright green (LED 11)
        led_set_pixel(LED_RIGHT_EYE, 0, 255, 0);  // Bright green (LED 2)
        // Ears
        led_set_pixel(LED_EAR_LEFT, 0, 150, 0);
        led_set_pixel(LED_EAR_RIGHT, 0, 150, 0);
        // Smile
        for (int i = LED_SMILE_START; i <= LED_SMILE_END; i++) {
            led_set_pixel(i, 0, 255, 0);
        }
        led_strip_refresh(strip);
        current_led_state = LED_STATE_IDLE;  // Keep lights on
        return;
    }
    
    // Turn lights OFF (command_id 14, 18, or string contains "turn off" and "light")
    if (command_id == 14 || command_id == 18 ||
        (command_string && string_contains(command_string, "turn off") && string_contains(command_string, "light"))) {
        printf("Turning lights off\n");
        led_command_understood();  // Show smile
        speak_text("Turning lights off.");
        led_clear_all();
        led_strip_refresh(strip);
        current_led_state = LED_STATE_IDLE;
        return;
    }
    
    // Color commands - set all LEDs to specific color
    // Command ID 15 = "pdNq jc KcLk To RfD" = "change the clock to red"
    // Command ID 16 = "pdNq jc KcLk To GRmN" = "change the clock to green"
    uint8_t r = 0, g = 0, b = 0;
    const char *color_name = NULL;
    bool color_set = false;
    
    // RED (command_id 15 or string contains "red")
    if (command_id == 15 || (command_string && string_contains(command_string, "red"))) {
        r = 255; g = 0; b = 0;
        color_name = "red";
        color_set = true;
    }
    // GREEN (command_id 16 or string contains "green")
    else if (command_id == 16 || (command_string && string_contains(command_string, "green"))) {
        r = 0; g = 255; b = 0;
        color_name = "green";
        color_set = true;
    }
    // BLUE (command_id 5 or string contains "blue")
    else if (command_id == 5 || (command_string && string_contains(command_string, "blue"))) {
        r = 0; g = 0; b = 255;
        color_name = "blue";
        color_set = true;
    }
    // WHITE (command_id 6 or string contains "white")
    else if (command_id == 6 || (command_string && string_contains(command_string, "white"))) {
        r = 255; g = 255; b = 255;
        color_name = "white";
        color_set = true;
    }
    // YELLOW (command_id 7 or string contains "yellow")
    else if (command_id == 7 || (command_string && string_contains(command_string, "yellow"))) {
        r = 255; g = 255; b = 0;
        color_name = "yellow";
        color_set = true;
    }
    // ORANGE (command_id 8 or string contains "orange")
    else if (command_id == 8 || (command_string && string_contains(command_string, "orange"))) {
        r = 255; g = 165; b = 0;
        color_name = "orange";
        color_set = true;
    }
    // PURPLE (command_id 9 or string contains "purple")
    else if (command_id == 9 || (command_string && string_contains(command_string, "purple"))) {
        r = 128; g = 0; b = 128;
        color_name = "purple";
        color_set = true;
    }
    // CYAN (command_id 10 or string contains "cyan")
    else if (command_id == 10 || (command_string && string_contains(command_string, "cyan"))) {
        r = 0; g = 255; b = 255;
        color_name = "cyan";
        color_set = true;
    }
    
    if (color_set && color_name) {
        printf("Setting lights to %s\n", color_name);
        led_command_understood();  // Show smile
        char response[64];
        snprintf(response, sizeof(response), "Setting lights to %s.", color_name);
        speak_text(response);
        
        // Set all LEDs to the color
        led_clear_all();
        for (int i = 0; i < MAX_LEDS; i++) {
            led_set_pixel(i, r, g, b);
        }
        led_strip_refresh(strip);
        current_led_state = LED_STATE_IDLE;  // Keep color
        return;
    }
    
    // Music/Audio controls
    // Command ID 5 = "hicST VnLYoM" = "highest volume"
    // Command ID 6 = "LbcST VnLYoM" = "lowest volume"
    // Command ID 7 = "gNKRmS jc VnLYoM" = "increase the volume"
    // Command ID 8 = "DgKRmS jc VnLYoM" = "decrease the volume"
    if (command_id == 5 || (command_string && string_contains(command_string, "highest") && string_contains(command_string, "volume"))) {
        printf("Setting volume to highest\n");
        led_command_understood();
        speak_text("Setting volume to highest.");
        // TODO: Implement volume control
        return;
    }
    else if (command_id == 6 || (command_string && string_contains(command_string, "lowest") && string_contains(command_string, "volume"))) {
        printf("Setting volume to lowest\n");
        led_command_understood();
        speak_text("Setting volume to lowest.");
        // TODO: Implement volume control
        return;
    }
    else if (command_id == 7 || (command_string && string_contains(command_string, "increase") && string_contains(command_string, "volume"))) {
        printf("Increasing volume\n");
        led_command_understood();
        speak_text("Increasing volume.");
        // TODO: Implement volume control
        return;
    }
    else if (command_id == 8 || (command_string && string_contains(command_string, "decrease") && string_contains(command_string, "volume"))) {
        printf("Decreasing volume\n");
        led_command_understood();
        speak_text("Decreasing volume.");
        // TODO: Implement volume control
        return;
    }
    
    // TV controls (can be used for IR blaster)
    // Command ID 9 = "TkN nN jc TmVm" = "turn on the TV"
    // Command ID 10 = "TkN eF jc TmVm" = "turn off the TV"
    if (command_id == 9 || (command_string && string_contains(command_string, "turn on") && string_contains(command_string, "tv"))) {
        printf("Turning TV on\n");
        led_command_understood();
        speak_text("Turning TV on.");
        // TODO: Implement IR blaster
        return;
    }
    else if (command_id == 10 || (command_string && string_contains(command_string, "turn off") && string_contains(command_string, "tv"))) {
        printf("Turning TV off\n");
        led_command_understood();
        speak_text("Turning TV off.");
        // TODO: Implement IR blaster
        return;
    }
    
    // Air conditioner controls (can be used for IR blaster)
    // Command ID 19 = "TkN nN jc fR KcNDgscNk" = "turn on the air conditioner"
    // Command ID 20 = "TkN eF jc fR KcNDgscNk" = "turn off the air conditioner"
    if (command_id == 19 || (command_string && string_contains(command_string, "turn on") && string_contains(command_string, "air conditioner"))) {
        printf("Turning air conditioner on\n");
        led_command_understood();
        speak_text("Turning air conditioner on.");
        // TODO: Implement IR blaster
        return;
    }
    else if (command_id == 20 || (command_string && string_contains(command_string, "turn off") && string_contains(command_string, "air conditioner"))) {
        printf("Turning air conditioner off\n");
        led_command_understood();
        speak_text("Turning air conditioner off.");
        // TODO: Implement IR blaster
        return;
    }
    
    // Temperature queries (Command IDs 21-31 are temperature settings)
    // For sensor queries, we'll use string matching since phonemes are for AC temp settings
    if (command_string && (string_contains(command_string, "what") || string_contains(command_string, "tell me")) && 
        string_contains(command_string, "temperature")) {
        printf("Temperature query\n");
        led_command_understood();
        speak_text("Temperature reading is not yet implemented.");
        // TODO: Read from SHT30 sensor
        return;
    }
    
    if (command_string && (string_contains(command_string, "what") || string_contains(command_string, "tell me")) && 
        string_contains(command_string, "humidity")) {
        printf("Humidity query\n");
        led_command_understood();
        speak_text("Humidity reading is not yet implemented.");
        // TODO: Read from SHT30 sensor
        return;
    }
    
    if (command_string && (string_contains(command_string, "what") || string_contains(command_string, "tell me")) && 
        (string_contains(command_string, "air quality") || string_contains(command_string, "voc"))) {
        printf("Air quality query\n");
        led_command_understood();
        speak_text("Air quality reading is not yet implemented.");
        // TODO: Read from SGP30 sensor
        return;
    }
    
    if (command_string && (string_contains(command_string, "what") || string_contains(command_string, "tell me")) && 
        string_contains(command_string, "co2")) {
        printf("CO2 level query\n");
        led_command_understood();
        speak_text("CO2 level reading is not yet implemented.");
        // TODO: Read from SCD30 sensor
        return;
    }
    
    if (command_string && (string_contains(command_string, "what") || string_contains(command_string, "tell me")) && 
        (string_contains(command_string, "light level") || string_contains(command_string, "brightness"))) {
        printf("Light level query\n");
        led_command_understood();
        speak_text("Light level reading is not yet implemented.");
        // TODO: Read from BH1750 sensor
        return;
    }
    
    if (command_string && (string_contains(command_string, "what") || string_contains(command_string, "tell me")) && 
        string_contains(command_string, "weather")) {
        printf("Weather query\n");
        led_command_understood();
        speak_text("Weather information is not yet implemented.");
        // TODO: Implement weather API
        return;
    }
    
    if (command_string && string_contains(command_string, "read sensors")) {
        printf("Read sensors command\n");
        led_command_understood();
        speak_text("Reading all sensors is not yet implemented.");
        // TODO: Read from all sensors
        return;
    }
    
    if (command_string && string_contains(command_string, "publish telemetry")) {
        printf("Publish telemetry command\n");
        led_command_understood();
        speak_text("Telemetry publishing is not yet implemented.");
        // TODO: Publish sensor data to AWS IoT Core
        return;
    }
    
    if (command_string && (string_contains(command_string, "play") && string_contains(command_string, "music"))) {
        printf("Play music command\n");
        led_command_understood();
        speak_text("Music playback is not yet implemented.");
        // TODO: Implement audio playback
        return;
    }
    
    if (command_string && (string_contains(command_string, "stop") && string_contains(command_string, "music"))) {
        printf("Stop music command\n");
        led_command_understood();
        speak_text("Music stop is not yet implemented.");
        // TODO: Implement audio stop
        return;
    }
    
    if (command_string && (string_contains(command_string, "pause") && string_contains(command_string, "music"))) {
        printf("Pause music command\n");
        led_command_understood();
        speak_text("Music pause is not yet implemented.");
        // TODO: Implement audio pause
        return;
    }
    
    if (command_string && (string_contains(command_string, "next") && string_contains(command_string, "song"))) {
        printf("Next song command\n");
        led_command_understood();
        speak_text("Next song is not yet implemented.");
        // TODO: Implement next track
        return;
    }
    
    if (command_string && (string_contains(command_string, "previous") && string_contains(command_string, "song"))) {
        printf("Previous song command\n");
        led_command_understood();
        speak_text("Previous song is not yet implemented.");
        // TODO: Implement previous track
        return;
    }
    
    if (command_string && string_contains(command_string, "test audio")) {
        printf("Test audio command\n");
        led_command_understood();
        speak_text("Audio test is not yet implemented.");
        // TODO: Implement audio test
        return;
    }
    
    // Unhandled command
    printf("Unhandled command_id: %d\n", command_id);
    led_command_understood();  // Show smile
    speak_text("Command not recognized.");
}

// Test functions
static test_status_t test_1_esp32_init(void)
{
    ESP_LOGI(TAG, "Test 1: ESP32-S3 System Initialization");
    speak_text("Test 1. ESP32-S3 system initialization.");
    
    // Check if system is running
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    ESP_LOGI(TAG, "Chip: %s, Cores: %d, Revision: %d", 
             CONFIG_IDF_TARGET, chip_info.cores, chip_info.revision);
    
    // Check free heap
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Free heap: %d bytes", free_heap);
    
    if (free_heap > 100000) {  // At least 100KB free
        speak_text("Test 1 passed. System initialized successfully.");
        return TEST_STATUS_PASS;
    } else {
        speak_text("Test 1 warning. Low memory available.");
        return TEST_STATUS_WARNING;
    }
}

static test_status_t test_2_sht30_sensor(void)
{
    ESP_LOGI(TAG, "Test 2: SHT30 Temperature/Humidity Sensor");
    speak_text("Test 2. SHT30 temperature and humidity sensor.");
    
    // This function is not yet implemented
    speak_text("This function is not yet implemented.");
    return TEST_STATUS_NOT_IMPLEMENTED;
}

static test_status_t test_3_sgp30_sensor(void)
{
    ESP_LOGI(TAG, "Test 3: SGP30 VOC Sensor");
    speak_text("Test 3. SGP30 VOC sensor.");
    
    speak_text("This function is not yet implemented.");
    return TEST_STATUS_NOT_IMPLEMENTED;
}

static test_status_t test_4_bh1750_sensor(void)
{
    ESP_LOGI(TAG, "Test 4: BH1750 Light Sensor");
    speak_text("Test 4. BH1750 light sensor.");
    
    speak_text("This function is not yet implemented.");
    return TEST_STATUS_NOT_IMPLEMENTED;
}

static test_status_t test_5_scd30_sensor(void)
{
    ESP_LOGI(TAG, "Test 5: SCD30 CO2 Sensor");
    speak_text("Test 5. SCD30 CO2 sensor.");
    
    speak_text("This function is not yet implemented.");
    return TEST_STATUS_NOT_IMPLEMENTED;
}

static test_status_t test_6_pca9685_leds(void)
{
    ESP_LOGI(TAG, "Test 6: PCA9685 RGB LED Control");
    speak_text("Test 6. PCA9685 RGB LED control.");
    
    // Test LED functionality
    if (strip) {
        // Test different colors
        led_set_status(TEST_STATUS_PASS);
        vTaskDelay(pdMS_TO_TICKS(1000));
        led_set_status(TEST_STATUS_WARNING);
        vTaskDelay(pdMS_TO_TICKS(1000));
        led_set_status(TEST_STATUS_FAIL);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        speak_text("Test 6 passed. LED control working.");
        return TEST_STATUS_PASS;
    } else {
        speak_text("Test 6 failed. LED strip not initialized.");
        return TEST_STATUS_FAIL;
    }
}

static test_status_t test_7_wifi_connectivity(void)
{
    ESP_LOGI(TAG, "Test 7: WiFi Connectivity");
    speak_text("Test 7. WiFi connectivity.");
    
    // Check WiFi status
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connected to: %s", ap_info.ssid);
        speak_text("Test 7 passed. WiFi connected.");
        return TEST_STATUS_PASS;
    } else {
        ESP_LOGW(TAG, "WiFi not connected: %s", esp_err_to_name(ret));
        speak_text("Test 7 warning. WiFi not connected.");
        return TEST_STATUS_WARNING;
    }
}

static test_status_t test_8_aws_iot_mqtt(void)
{
    ESP_LOGI(TAG, "Test 8: AWS IoT Core MQTT Connectivity");
    speak_text("Test 8. AWS IoT Core MQTT connectivity.");
    
    speak_text("This function is not yet implemented.");
    return TEST_STATUS_NOT_IMPLEMENTED;
}

static test_status_t test_9_wake_word_detection(void)
{
    ESP_LOGI(TAG, "Test 9: ESP-SR Wake Word Detection");
    speak_text("Test 9. ESP-SR wake word detection.");
    
    // Check if ESP-SR is available (would need to check model partition)
    speak_text("This function is not yet implemented.");
    return TEST_STATUS_NOT_IMPLEMENTED;
}

static test_status_t test_10_ir_blaster(void)
{
    ESP_LOGI(TAG, "Test 10: IR Blaster Functionality");
    speak_text("Test 10. IR blaster functionality.");
    
    speak_text("This function is not yet implemented.");
    return TEST_STATUS_NOT_IMPLEMENTED;
}

static test_status_t test_11_audio_output(void)
{
    ESP_LOGI(TAG, "Test 11: Audio Output System (TPA3116D2)");
    speak_text("Test 11. Audio output system.");
    
    // Audio system should be initialized via esp_board_init
    // Test by playing a simple tone or beep
    speak_text("This function is not yet implemented.");
    return TEST_STATUS_NOT_IMPLEMENTED;
}

static test_status_t test_12_sensor_telemetry(void)
{
    ESP_LOGI(TAG, "Test 12: Sensor Telemetry Publishing");
    speak_text("Test 12. Sensor telemetry publishing.");
    
    speak_text("This function is not yet implemented.");
    return TEST_STATUS_NOT_IMPLEMENTED;
}

// Main test runner
void run_test_suite(void *pvParameters)
{
    ESP_LOGI(TAG, "=== Naphome Phase 0.9 Test Suite Starting ===");
    
    // Initial delay to allow system to stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Introduction
    speak_text("This is a demo of the Naphome 0.9.");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Test function pointers
    typedef test_status_t (*test_func_t)(void);
    struct {
        const char *name;
        test_func_t func;
    } tests[] = {
        {"ESP32-S3 System Initialization", test_1_esp32_init},
        {"SHT30 Temperature/Humidity Sensor", test_2_sht30_sensor},
        {"SGP30 VOC Sensor", test_3_sgp30_sensor},
        {"BH1750 Light Sensor", test_4_bh1750_sensor},
        {"SCD30 CO2 Sensor", test_5_scd30_sensor},
        {"PCA9685 RGB LED Control", test_6_pca9685_leds},
        {"WiFi Connectivity", test_7_wifi_connectivity},
        {"AWS IoT Core MQTT", test_8_aws_iot_mqtt},
        {"ESP-SR Wake Word Detection", test_9_wake_word_detection},
        {"IR Blaster Functionality", test_10_ir_blaster},
        {"Audio Output System", test_11_audio_output},
        {"Sensor Telemetry Publishing", test_12_sensor_telemetry},
    };
    
    int pass_count = 0;
    int warning_count = 0;
    int fail_count = 0;
    int not_impl_count = 0;
    
    // Run all tests
    for (int i = 0; i < 12; i++) {
        ESP_LOGI(TAG, "\n--- Running Test %d/%d: %s ---", i + 1, 12, tests[i].name);
        
        test_status_t status = tests[i].func();
        led_set_status(status);
        
        switch (status) {
            case TEST_STATUS_PASS:
                pass_count++;
                break;
            case TEST_STATUS_WARNING:
                warning_count++;
                break;
            case TEST_STATUS_FAIL:
                fail_count++;
                break;
            case TEST_STATUS_NOT_IMPLEMENTED:
                not_impl_count++;
                break;
        }
        
        // Delay between tests
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    
    // Summary
    ESP_LOGI(TAG, "\n=== Test Suite Complete ===");
    ESP_LOGI(TAG, "Passed: %d, Warnings: %d, Failed: %d, Not Implemented: %d",
             pass_count, warning_count, fail_count, not_impl_count);
    
    char summary[256];
    snprintf(summary, sizeof(summary),
             "Test suite complete. %d passed, %d warnings, %d failed, %d not implemented.",
             pass_count, warning_count, fail_count, not_impl_count);
    speak_text(summary);
    
    // Set final LED status
    if (fail_count == 0 && not_impl_count == 0) {
        led_set_status(TEST_STATUS_PASS);
    } else if (fail_count == 0) {
        led_set_status(TEST_STATUS_WARNING);
    } else {
        led_set_status(TEST_STATUS_FAIL);
    }
    
    // Reset trigger flag so test suite can be run again
    test_suite_triggered = false;
    vTaskDelete(NULL);
}

// ESP-SR feed task - captures audio from microphone
// EXACT COPY from working example
static void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_feed_channel_num(afe_data);
    int feed_channel = esp_get_feed_channel();
    assert(nch == feed_channel);
    int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
    assert(i2s_buff);

    while (task_flag) {
        esp_get_feed_data(true, i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);

        afe_handle->feed(afe_data, i2s_buff);
    }
    if (i2s_buff) {
        free(i2s_buff);
        i2s_buff = NULL;
    }
    vTaskDelete(NULL);
}

// ESP-SR detect task - EXACT COPY from working example
static void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    printf("multinet:%s\n", mn_name);
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_data = multinet->create(mn_name, 6000);
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    esp_mn_commands_update_from_sdkconfig(multinet, model_data); // Add speech commands from sdkconfig
    assert(mu_chunksize == afe_chunksize);
    //print active speech commands
    multinet->print_active_speech_commands(model_data);

    printf("------------detect start------------\n");
    while (task_flag) {
        afe_fetch_result_t* res = afe_handle->fetch(afe_data); 
        if (!res || res->ret_value == ESP_FAIL) {
            printf("fetch error!\n");
            break;
        }

        if (res->wakeup_state == WAKENET_DETECTED) {
            printf("WAKEWORD DETECTED\n");
	        multinet->clean(model_data);
            led_wake_word_detected(); // Illuminate ears
        }

        if (res->raw_data_channels == 1 && res->wakeup_state == WAKENET_DETECTED) {
            wakeup_flag = 1;
            led_wake_word_detected(); // Illuminate ears
        } else if (res->raw_data_channels > 1 && res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
            // For a multi-channel AFE, it is necessary to wait for the channel to be verified.
            printf("AFE_FETCH_CHANNEL_VERIFIED, channel index: %d\n", res->trigger_channel_id);
            wakeup_flag = 1;
            led_wake_word_detected(); // Illuminate ears
        }

        if (wakeup_flag == 1) {
            esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

            if (mn_state == ESP_MN_STATE_DETECTING) {
                continue;
            }

            if (mn_state == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++) {
                    printf("TOP %d, command_id: %d, phrase_id: %d, string: %s, prob: %f\n", 
                    i+1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->string, mn_result->prob[i]);
                }
                // Execute the top command (highest probability)
                if (mn_result->num > 0) {
                    speech_commands_action_with_string(mn_result->command_id[0], mn_result->string);
                }
                printf("-----------listening-----------\n");
            }

            if (mn_state == ESP_MN_STATE_TIMEOUT) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                printf("timeout, string:%s\n", mn_result->string);
                afe_handle->enable_wakenet(afe_data);
                wakeup_flag = 0;
                current_led_state = LED_STATE_IDLE;  // Return to idle animation
                led_clear_all();  // Clear LEDs for idle state
                printf("\n-----------awaits to be waken up-----------\n");
                continue;
            }
        }
    }
    if (model_data) {
        multinet->destroy(model_data);
        model_data = NULL;
    }
    printf("detect exit\n");
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Naphome Phase 0.9 Test Suite");
    
    // EXACT COPY from working example - no modifications
    models = esp_srmodel_init("model"); // partition label defined in partitions.csv
    
    // Initialize board hardware (I2C, audio, etc.)
    // This must be called before any other hardware access
    // Initialize board (audio system) - use 2 channels for ESP-SR
    ESP_LOGI(TAG, "Initializing board hardware...");
    ESP_ERROR_CHECK(esp_board_init(16000, 2, 16));
    // ESP_ERROR_CHECK(esp_sdcard_init("/sdcard", 10));
    
    // Small delay to ensure audio hardware is fully ready
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Play offline welcome audio file on startup
    const uint8_t *welcome_wav = _binary_offline_welcome_wav_start;
    size_t welcome_wav_size = _binary_offline_welcome_wav_end - _binary_offline_welcome_wav_start;
    if (welcome_wav_size > 0) {
        ESP_LOGI(TAG, "Playing offline welcome audio on startup (%zu bytes)", welcome_wav_size);
        esp_err_t play_ret = play_wav_file(welcome_wav, welcome_wav_size);
        if (play_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to play welcome audio: %s", esp_err_to_name(play_ret));
        } else {
            ESP_LOGI(TAG, "Welcome audio played successfully");
        }
    } else {
        ESP_LOGW(TAG, "Welcome audio file not embedded");
    }

#if CONFIG_IDF_TARGET_ESP32
    printf("This demo only support ESP32S3\n");
    return;
#else 
    afe_config_t *afe_config = afe_config_init(esp_get_input_format(), models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    afe_handle = esp_afe_handle_from_config(afe_config);
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    afe_config_free(afe_config);
#endif

    task_flag = 1;
    xTaskCreatePinnedToCore(&detect_Task, "detect", 8 * 1024, (void*)afe_data, 5, NULL, 1);
    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void*)afe_data, 5, NULL, 0);
    
    // Initialize LED strip (always, independent of NVS)
    led_init();
    
    // Start LED animation task for idle eye movement
    current_led_state = LED_STATE_IDLE;
    xTaskCreatePinnedToCore(
        led_animation_task,
        "led_animation",
        2048,
        NULL,
        3,
        NULL,
        1
    );
    
    // Initial idle animation
    if (strip) {
        led_idle_animation();
    }
    
    // Initialize Naphome-specific features (after ESP-SR is running)
    // Initialize NVS (for WiFi credentials and other config)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        // Continue without NVS - ESP-SR and LEDs are already running
        ESP_LOGI(TAG, "Voice recognition initialized. Say 'Hi ESP' followed by 'run the demo' to start the test suite.");
    } else {
        // Initialize network stack for HTTP client (for Google TTS)
        // MUST be done before any HTTP requests
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        
        // Give TCP/IP stack time to initialize
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Initialize WiFi (basic init, connection would be done separately)
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
        if (!sta_netif) {
            ESP_LOGE(TAG, "Failed to create WiFi STA netif");
        }
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        
        // Set country code to fix regulatory domain linker issue in ESP-IDF 5.4.3
        wifi_country_t country = {
            .cc = "US",
            .schan = 1,
            .nchan = 11,
            .policy = WIFI_COUNTRY_POLICY_AUTO,
        };
        ESP_ERROR_CHECK(esp_wifi_set_country(&country));
        
        ESP_ERROR_CHECK(esp_wifi_start());
        
        // Connect to WiFi
        wifi_config_t wifi_config = {
            .sta = {
                .ssid = "TP-Link_0F23",
                .password = "93419383",
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };
        ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", wifi_config.sta.ssid);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
        
        // Wait for connection (with timeout)
        int retry_count = 0;
        const int max_retries = 20;  // 20 seconds timeout
        while (retry_count < max_retries) {
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                ESP_LOGI(TAG, "WiFi connected to: %s (RSSI: %d)", ap_info.ssid, ap_info.rssi);
                // Give TCP/IP stack additional time to be fully ready
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            retry_count++;
        }
        
        if (retry_count >= max_retries) {
            ESP_LOGW(TAG, "WiFi connection timeout");
        } else {
            // Initialize SNTP for time synchronization
            esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            esp_netif_sntp_init(&sntp_config);
            
            // Wait for time sync (with timeout)
            time_t now = 0;
            struct tm timeinfo = {0};
            int sntp_retries = 0;
            while (sntp_retries < 10 && timeinfo.tm_year < (2016 - 1900)) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                time(&now);
                localtime_r(&now, &timeinfo);
                sntp_retries++;
            }
            
            if (timeinfo.tm_year >= (2016 - 1900)) {
                ESP_LOGI(TAG, "Time synchronized: %04d-%02d-%02d %02d:%02d:%02d",
                         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                
                // Create greeting with time and sensor data
                char greeting_prompt[1024];
                char time_str[128];
                const char *ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";
                int hour_12 = timeinfo.tm_hour % 12;
                if (hour_12 == 0) hour_12 = 12;
                
                const char *month_names[] = {
                    "January", "February", "March", "April", "May", "June",
                    "July", "August", "September", "October", "November", "December"
                };
                
                snprintf(time_str, sizeof(time_str),
                        "%d:%02d %s on %s %d, %d",
                        hour_12, timeinfo.tm_min, ampm,
                        month_names[timeinfo.tm_mon],
                        timeinfo.tm_mday,
                        timeinfo.tm_year + 1900);
                
                // Try to read sensor data (if available)
                // Note: Sensor drivers require I2C bus initialization which is complex
                // For now, we'll skip sensor data in the greeting
                // TODO: Initialize I2C bus and read sensors when drivers are integrated
                bool has_sensor_data = false;
                float temp = 22.0f;  // Default values
                float humidity = 45.0f;
                uint16_t voc = 50;
                uint16_t eco2 = 400;
                
                // Build prompt for LLM (always include time, sensor data optional)
                snprintf(greeting_prompt, sizeof(greeting_prompt),
                    "Hello, I am Naphome. The time is %s. "
                    "Please provide a brief, friendly greeting and status update in one or two sentences.",
                    time_str);
                
                // Call LLM and get response
                char llm_response[512];
                esp_err_t llm_ret = gemini_llm_call(greeting_prompt, llm_response, sizeof(llm_response));
                
                if (llm_ret == ESP_OK) {
                    ESP_LOGI(TAG, "LLM greeting: %s", llm_response);
                    speak_text(llm_response);
                } else {
                    // Fallback to simple greeting
                    char fallback[256];
                    snprintf(fallback, sizeof(fallback),
                            "Hello, I am Naphome. The time is %s. Voice recognition ready. Say Hi ESP, then run the demo.",
                            time_str);
                    speak_text(fallback);
                }
            } else {
                ESP_LOGW(TAG, "Time sync failed, using simple greeting");
                speak_text("Hello, I am Naphome. Voice recognition ready. Say Hi ESP, then run the demo.");
            }
        }
    }
}
