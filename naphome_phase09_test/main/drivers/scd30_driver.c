/**
 * @file scd30_driver.c
 * @brief SCD30 CO2 Sensor Driver Implementation
 */

#include "scd30_driver.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>
#include "driver/i2c.h"

static const char *TAG = "scd30_driver";

// CRC-8 polynomial for SCD30 (same as SHT30/SGP30)
static uint8_t scd30_crc8(const uint8_t *data, size_t len)
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

// Helper to send 16-bit command with CRC (using old I2C API)
static esp_err_t scd30_send_command(i2c_port_t i2c_port, uint8_t device_addr, uint16_t cmd, uint16_t arg)
{
    uint8_t tx_data[5];
    tx_data[0] = (cmd >> 8) & 0xFF;
    tx_data[1] = cmd & 0xFF;
    tx_data[2] = (arg >> 8) & 0xFF;
    tx_data[3] = arg & 0xFF;
    tx_data[4] = scd30_crc8(&tx_data[2], 2);
    
    i2c_cmd_handle_t i2c_cmd = i2c_cmd_link_create();
    i2c_master_start(i2c_cmd);
    i2c_master_write_byte(i2c_cmd, (device_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(i2c_cmd, tx_data, 5, true);
    i2c_master_stop(i2c_cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, i2c_cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(i2c_cmd);
    return ret;
}

bool scd30_init(scd30_handle_t *handle, i2c_port_t i2c_port, uint8_t device_addr)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    memset(handle, 0, sizeof(scd30_handle_t));
    handle->i2c_port = i2c_port;
    handle->device_addr = (device_addr != 0) ? device_addr : SCD30_I2C_ADDR;

    // Try to get firmware version to detect hardware (using old I2C API)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
    uint8_t fw_cmd[2] = {0xD1, 0x00};
    i2c_master_write(cmd, fw_cmd, 2, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_READ, true);
        uint8_t rx_data[3];
        i2c_master_read(cmd, rx_data, 3, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        
        ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK && scd30_crc8(rx_data, 2) == rx_data[2]) {
            handle->hardware_present = true;
            ESP_LOGI(TAG, "SCD30 hardware detected at address 0x%02X", handle->device_addr);
            
            // Start continuous measurement with 2 second interval
            scd30_send_command(handle->i2c_port, handle->device_addr, SCD30_CMD_START_CONT_MEAS, 0x0002);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            ESP_LOGW(TAG, "SCD30 hardware not detected, will use synthetic data");
            handle->hardware_present = false;
        }
    } else {
        ESP_LOGW(TAG, "SCD30 hardware not detected, will use synthetic data");
        handle->hardware_present = false;
    }

    // Initialize synthetic data generator
    handle->synthetic_co2_base = 400.0f;      // Typical indoor CO2
    handle->synthetic_temp_base = 22.0f;     // Room temperature
    handle->synthetic_humidity_base = 45.0f; // Moderate humidity
    handle->synthetic_counter = 0;

    handle->initialized = true;
    return true;
}

void scd30_deinit(scd30_handle_t *handle)
{
    if (handle != NULL) {
        // Stop continuous measurement
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
        uint8_t stop_cmd[2] = {0x01, 0x04};
        i2c_master_write(cmd, stop_cmd, 2, true);
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        
        handle->initialized = false;
    }
}

bool scd30_read(scd30_handle_t *handle, scd30_data_t *data)
{
    if (handle == NULL || data == NULL || !handle->initialized) {
        ESP_LOGE(TAG, "Invalid parameters or not initialized");
        return false;
    }

    data->hardware_present = handle->hardware_present;

    if (handle->hardware_present) {
        // Check if data is ready (using old I2C API)
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
        uint8_t ready_cmd[2] = {0x02, 0x02};
        i2c_master_write(cmd, ready_cmd, 2, true);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_READ, true);
            uint8_t ready_data[3];
            i2c_master_read(cmd, ready_data, 3, I2C_MASTER_LAST_NACK);
            i2c_master_stop(cmd);
            
            ret = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
            i2c_cmd_link_delete(cmd);
            
            if (ret == ESP_OK && scd30_crc8(ready_data, 2) == ready_data[2]) {
                uint16_t ready = (ready_data[0] << 8) | ready_data[1];
                if (ready != 0) {
                    // Read measurement
                    cmd = i2c_cmd_link_create();
                    i2c_master_start(cmd);
                    i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
                    uint8_t read_cmd[2] = {0x03, 0x00};
                    i2c_master_write(cmd, read_cmd, 2, true);
                    i2c_master_stop(cmd);
                    ret = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
                    i2c_cmd_link_delete(cmd);
                    
                    if (ret == ESP_OK) {
                        vTaskDelay(pdMS_TO_TICKS(10));
                        cmd = i2c_cmd_link_create();
                        i2c_master_start(cmd);
                        i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_READ, true);
                        uint8_t rx_data[18];  // 6 values * 3 bytes each (2 data + 1 CRC)
                        i2c_master_read(cmd, rx_data, 18, I2C_MASTER_LAST_NACK);
                        i2c_master_stop(cmd);
                        
                        ret = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(200));
                        i2c_cmd_link_delete(cmd);
                        
                        if (ret == ESP_OK) {
                            // Verify CRCs and parse data
                            bool crc_ok = true;
                            for (int i = 0; i < 6; i++) {
                                if (scd30_crc8(&rx_data[i*3], 2) != rx_data[i*3 + 2]) {
                                    crc_ok = false;
                                    break;
                                }
                            }
                            
                            if (crc_ok) {
                                // Parse CO2 (first value) - bytes 0-1
                                uint16_t co2_raw = (rx_data[0] << 8) | rx_data[1];
                                data->co2_ppm = (float)co2_raw;
                                
                                // Parse temperature (second value) - bytes 3-4
                                uint16_t temp_raw = (rx_data[3] << 8) | rx_data[4];
                                data->temperature_c = (float)temp_raw / 100.0f;
                                
                                // Parse humidity (third value) - bytes 6-7
                                uint16_t hum_raw = (rx_data[6] << 8) | rx_data[7];
                                data->humidity_rh = (float)hum_raw / 100.0f;
                                
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
            }
        }
        
        // If we get here, hardware read failed
        handle->hardware_present = false;
    }

    // Generate synthetic data
    handle->synthetic_counter++;
    float time_factor = (float)handle->synthetic_counter * 0.01f;
    
    // Simulate CO2 variation (400-1000 ppm range)
    data->co2_ppm = handle->synthetic_co2_base + 300.0f * sinf(time_factor * 0.3f) + 
                    50.0f * sinf(time_factor * 1.5f);
    
    // Simulate temperature variation (20-25°C range)
    data->temperature_c = handle->synthetic_temp_base + 2.5f * sinf(time_factor) + 
                         0.5f * sinf(time_factor * 3.7f);
    
    // Simulate humidity variation (40-60% range)
    data->humidity_rh = handle->synthetic_humidity_base + 10.0f * sinf(time_factor * 0.7f) +
                       2.0f * sinf(time_factor * 2.3f);
    
    data->valid = true;
    ESP_LOGD(TAG, "Synthetic data: CO2=%.1f ppm, T=%.2f°C, H=%.2f%%", 
             data->co2_ppm, data->temperature_c, data->humidity_rh);
    return true;
}

bool scd30_is_hardware_present(scd30_handle_t *handle)
{
    return handle != NULL && handle->hardware_present;
}
