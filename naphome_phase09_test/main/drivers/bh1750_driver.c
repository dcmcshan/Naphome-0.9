/**
 * @file bh1750_driver.c
 * @brief BH1750 Ambient Light Sensor Driver Implementation
 */

#include "bh1750_driver.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>
#include "driver/i2c.h"

static const char *TAG = "bh1750_driver";

bool bh1750_init(bh1750_handle_t *handle, i2c_port_t i2c_port, uint8_t device_addr)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }

    memset(handle, 0, sizeof(bh1750_handle_t));
    handle->i2c_port = i2c_port;
    handle->device_addr = (device_addr != 0) ? device_addr : BH1750_I2C_ADDR;

    // Try power on and reset to detect hardware (using old I2C API)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
    uint8_t power_on = BH1750_CMD_POWER_ON;
    i2c_master_write(cmd, &power_on, 1, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
        uint8_t reset = BH1750_CMD_RESET;
        i2c_master_write(cmd, &reset, 1, true);
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Set continuous high resolution mode
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
        uint8_t cont_mode = BH1750_CMD_CONT_H_MODE;
        i2c_master_write(cmd, &cont_mode, 1, true);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            handle->hardware_present = true;
            ESP_LOGI(TAG, "BH1750 hardware detected at address 0x%02X", handle->device_addr);
        } else {
            ESP_LOGW(TAG, "BH1750 hardware not detected, will use synthetic data");
            handle->hardware_present = false;
        }
    } else {
        ESP_LOGW(TAG, "BH1750 hardware not detected, will use synthetic data");
        handle->hardware_present = false;
    }

    // Initialize synthetic data generator
    handle->synthetic_lux_base = 300.0f;  // Typical indoor lighting (300 lux)
    handle->synthetic_counter = 0;

    handle->initialized = true;
    return true;
}

void bh1750_deinit(bh1750_handle_t *handle)
{
    if (handle != NULL) {
        // Power down the sensor
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_WRITE, true);
        uint8_t power_down = BH1750_CMD_POWER_DOWN;
        i2c_master_write(cmd, &power_down, 1, true);
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        
        handle->initialized = false;
    }
}

bool bh1750_read(bh1750_handle_t *handle, bh1750_data_t *data)
{
    if (handle == NULL || data == NULL || !handle->initialized) {
        ESP_LOGE(TAG, "Invalid parameters or not initialized");
        return false;
    }

    data->hardware_present = handle->hardware_present;

    if (handle->hardware_present) {
        // Wait for measurement to complete
        vTaskDelay(pdMS_TO_TICKS(BH1750_MEASURE_DELAY_MS));
        
        // Read from real hardware using old I2C API
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (handle->device_addr << 1) | I2C_MASTER_READ, true);
        uint8_t rx_data[2];
        i2c_master_read(cmd, rx_data, 2, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            // Convert 16-bit value to lux
            uint16_t raw_value = (rx_data[0] << 8) | rx_data[1];
            data->lux = (float)raw_value / 1.2f;  // High resolution mode: divide by 1.2
            data->valid = true;
            return true;
        } else {
            ESP_LOGW(TAG, "I2C receive failed, using synthetic data");
            handle->hardware_present = false;
        }
    }

    // Generate synthetic data (simulate day/night cycle)
    handle->synthetic_counter++;
    float time_factor = (float)handle->synthetic_counter * 0.01f;
    
    // Simulate light variation (0-1000 lux range, with day/night cycle)
    // Day: 200-1000 lux, Night: 0-50 lux
    float day_night = sinf(time_factor * 0.1f);  // Slow day/night cycle
    if (day_night > 0) {
        // Daytime: 200-1000 lux
        data->lux = 200.0f + 800.0f * day_night + 100.0f * sinf(time_factor * 2.3f);
    } else {
        // Nighttime: 0-50 lux
        data->lux = 25.0f + 25.0f * fabsf(day_night) + 5.0f * sinf(time_factor * 1.7f);
    }
    
    // Ensure non-negative
    if (data->lux < 0.0f) {
        data->lux = 0.0f;
    }
    
    data->valid = true;
    ESP_LOGD(TAG, "Synthetic data: Lux=%.2f", data->lux);
    return true;
}

bool bh1750_is_hardware_present(bh1750_handle_t *handle)
{
    return handle != NULL && handle->hardware_present;
}
