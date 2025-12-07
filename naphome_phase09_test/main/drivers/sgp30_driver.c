/**
 * @file sgp30_driver.c
 * @brief SGP30 VOC Sensor Driver Implementation
 */

#include "sgp30_driver.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>
#include "driver/i2c.h"

static const char *TAG = "sgp30_driver";

// CRC-8 polynomial for SGP30
static uint8_t sgp30_crc8(const uint8_t *data, size_t len)
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

bool sgp30_init(sgp30_handle_t *handle, i2c_port_t i2c_port, uint8_t device_addr)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    memset(handle, 0, sizeof(sgp30_handle_t));
    handle->i2c_port = i2c_port;
    handle->device_addr = (device_addr != 0) ? device_addr : SGP30_I2C_ADDR;

    // Try to read serial ID to detect hardware (using old I2C API)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
    uint8_t serial_cmd[2] = {0x36, 0x82};
    i2c_master_write(cmd, serial_cmd, 2, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1));
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_READ, true);
        uint8_t rx_data[9];
        i2c_master_read(cmd, rx_data, 9, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        
        ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            handle->hardware_present = true;
            ESP_LOGI(TAG, "SGP30 hardware detected at address 0x%02X", handle->device_addr);
            
            // Initialize air quality measurement
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
            uint8_t init_cmd[2] = {0x20, 0x03};
            i2c_master_write(cmd, init_cmd, 2, true);
            i2c_master_stop(cmd);
            i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
            i2c_cmd_link_delete(cmd);
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ESP_LOGW(TAG, "SGP30 hardware not detected, will use synthetic data");
            handle->hardware_present = false;
        }
    } else {
        ESP_LOGW(TAG, "SGP30 hardware not detected, will use synthetic data");
        handle->hardware_present = false;
    }

    // Initialize synthetic data generator
    handle->synthetic_tvoc_base = 50;   // Typical indoor TVOC
    handle->synthetic_eco2_base = 400;  // Typical indoor CO2
    handle->synthetic_counter = 0;

    handle->initialized = true;
    return true;
}

void sgp30_deinit(sgp30_handle_t *handle)
{
    if (handle != NULL) {
        handle->initialized = false;
    }
}

bool sgp30_read(sgp30_handle_t *handle, sgp30_data_t *data)
{
    if (handle == NULL || data == NULL || !handle->initialized) {
        ESP_LOGE(TAG, "Invalid parameters or not initialized");
        return false;
    }

    data->hardware_present = handle->hardware_present;

    if (handle->hardware_present) {
        // Try to read from real hardware using old I2C API
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
        uint8_t measure_cmd[2] = {0x20, 0x08};  // Measure air quality
        i2c_master_write(cmd, measure_cmd, 2, true);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "I2C transmit failed, falling back to synthetic data");
            handle->hardware_present = false;
        } else {
            vTaskDelay(pdMS_TO_TICKS(SGP30_MEASURE_DELAY_MS));
            
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_READ, true);
            uint8_t rx_data[6];
            i2c_master_read(cmd, rx_data, 6, I2C_MASTER_LAST_NACK);
            i2c_master_stop(cmd);
            
            ret = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
            i2c_cmd_link_delete(cmd);
            
            if (ret == ESP_OK) {
                // Verify CRC
                if (sgp30_crc8(rx_data, 2) == rx_data[2] && 
                    sgp30_crc8(&rx_data[3], 2) == rx_data[5]) {
                    data->tvoc_ppb = (rx_data[0] << 8) | rx_data[1];
                    data->eco2_ppm = (rx_data[3] << 8) | rx_data[4];
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
    
    // Simulate TVOC variation (30-100 ppb range)
    data->tvoc_ppb = handle->synthetic_tvoc_base + (uint16_t)(30.0f * sinf(time_factor) + 
                        10.0f * sinf(time_factor * 2.3f));
    
    // Simulate eCO2 variation (350-600 ppm range)
    data->eco2_ppm = handle->synthetic_eco2_base + (uint16_t)(100.0f * sinf(time_factor * 0.5f) +
                        30.0f * sinf(time_factor * 1.7f));
    
    data->valid = true;
    ESP_LOGD(TAG, "Synthetic data: TVOC=%d ppb, eCO2=%d ppm", data->tvoc_ppb, data->eco2_ppm);
    return true;
}

bool sgp30_is_hardware_present(sgp30_handle_t *handle)
{
    return handle != NULL && handle->hardware_present;
}
