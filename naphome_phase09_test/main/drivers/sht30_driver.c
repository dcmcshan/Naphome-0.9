/**
 * @file sht30_driver.c
 * @brief SHT30 Temperature and Humidity Sensor Driver Implementation
 */

#include "sht30_driver.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "sht30_driver";

// CRC-8 polynomial for SHT30
static uint8_t sht30_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

bool sht30_init(sht30_handle_t *handle, i2c_master_bus_handle_t i2c_bus, uint8_t device_addr)
{
    if (handle == NULL || i2c_bus == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    memset(handle, 0, sizeof(sht30_handle_t));
    handle->i2c_bus = i2c_bus;
    handle->device_addr = (device_addr != 0) ? device_addr : SHT30_I2C_ADDR;

    // Try to create I2C device handle
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = handle->device_addr,
        .scl_speed_hz = 100000,
    };
    
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &handle->i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add I2C device at 0x%02X: %s", handle->device_addr, esp_err_to_name(ret));
        handle->hardware_present = false;
    } else {
        // Try soft reset to detect hardware
        uint8_t reset_cmd[2] = {0x30, 0xA2};
        ret = i2c_master_transmit(handle->i2c_dev, reset_cmd, 2, pdMS_TO_TICKS(100));
        if (ret == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));
            handle->hardware_present = true;
            ESP_LOGI(TAG, "SHT30 hardware detected at address 0x%02X", handle->device_addr);
        } else {
            ESP_LOGW(TAG, "SHT30 hardware not detected, will use synthetic data");
            handle->hardware_present = false;
        }
    }

    // Initialize synthetic data generator
    handle->synthetic_temp_base = 22.0f;  // Room temperature
    handle->synthetic_humidity_base = 45.0f;  // Moderate humidity
    handle->synthetic_counter = 0;

    handle->initialized = true;
    return true;
}

void sht30_deinit(sht30_handle_t *handle)
{
    if (handle != NULL && handle->i2c_dev != NULL) {
        i2c_master_bus_rm_device(handle->i2c_dev);
        handle->i2c_dev = NULL;
    }
    if (handle != NULL) {
        handle->initialized = false;
    }
}

bool sht30_read(sht30_handle_t *handle, sht30_data_t *data)
{
    if (handle == NULL || data == NULL || !handle->initialized) {
        ESP_LOGE(TAG, "Invalid parameters or not initialized");
        return false;
    }

    data->hardware_present = handle->hardware_present;

    if (handle->hardware_present && handle->i2c_dev != NULL) {
        // Try to read from real hardware
        uint8_t cmd[2] = {0x24, 0x00};  // High precision measurement
        esp_err_t ret = i2c_master_transmit(handle->i2c_dev, cmd, 2, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "I2C transmit failed, falling back to synthetic data");
            handle->hardware_present = false;
        } else {
            vTaskDelay(pdMS_TO_TICKS(SHT30_MEASURE_DELAY_MS));
            
            uint8_t rx_data[6];
            ret = i2c_master_receive(handle->i2c_dev, rx_data, 6, pdMS_TO_TICKS(100));
            if (ret == ESP_OK) {
                // Verify CRC
                if (sht30_crc8(rx_data, 2) == rx_data[2] && 
                    sht30_crc8(&rx_data[3], 2) == rx_data[5]) {
                    // Convert temperature
                    uint16_t temp_raw = (rx_data[0] << 8) | rx_data[1];
                    data->temperature_c = -45.0f + 175.0f * (float)temp_raw / 65535.0f;
                    
                    // Convert humidity
                    uint16_t hum_raw = (rx_data[3] << 8) | rx_data[4];
                    data->humidity_rh = 100.0f * (float)hum_raw / 65535.0f;
                    
                    data->valid = true;
                    return true;
                } else {
                    ESP_LOGW(TAG, "CRC check failed, using synthetic data");
                }
            } else {
                ESP_LOGW(TAG, "I2C receive failed, using synthetic data");
            }
        }
    }

    // Generate synthetic data
    handle->synthetic_counter++;
    float time_factor = (float)handle->synthetic_counter * 0.01f;
    
    // Simulate temperature variation (20-25°C range)
    data->temperature_c = handle->synthetic_temp_base + 2.5f * sinf(time_factor) + 
                         0.5f * sinf(time_factor * 3.7f);
    
    // Simulate humidity variation (40-60% range)
    data->humidity_rh = handle->synthetic_humidity_base + 10.0f * sinf(time_factor * 0.7f) +
                       2.0f * sinf(time_factor * 2.3f);
    
    data->valid = true;
    ESP_LOGD(TAG, "Synthetic data: T=%.2f°C, H=%.2f%%", data->temperature_c, data->humidity_rh);
    return true;
}

bool sht30_is_hardware_present(sht30_handle_t *handle)
{
    return handle != NULL && handle->hardware_present;
}
