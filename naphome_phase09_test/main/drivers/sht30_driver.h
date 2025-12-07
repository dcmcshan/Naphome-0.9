/**
 * @file sht30_driver.h
 * @brief SHT30 Temperature and Humidity Sensor Driver
 * 
 * I2C Address: 0x44
 * Supports synthetic data generation if hardware is not present
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHT30_I2C_ADDR 0x44
#define SHT30_CMD_MEASURE_HPM 0x2400  // High precision, clock stretching
#define SHT30_CMD_SOFT_RESET 0x30A2
#define SHT30_MEASURE_DELAY_MS 15

typedef struct {
    float temperature_c;    // Temperature in Celsius
    float humidity_rh;      // Relative humidity in %
    bool valid;             // True if data is valid
    bool hardware_present;  // True if real hardware detected
} sht30_data_t;

typedef struct {
    i2c_port_t i2c_port;
    uint8_t device_addr;
    bool initialized;
    bool hardware_present;
    // Synthetic data generation
    float synthetic_temp_base;
    float synthetic_humidity_base;
    uint32_t synthetic_counter;
} sht30_handle_t;

/**
 * @brief Initialize SHT30 sensor
 * @param handle Pointer to SHT30 handle structure
 * @param i2c_port I2C port number (I2C_NUM_0 or I2C_NUM_1)
 * @param device_addr I2C device address (default: SHT30_I2C_ADDR)
 * @return true if initialized successfully, false otherwise
 */
bool sht30_init(sht30_handle_t *handle, i2c_port_t i2c_port, uint8_t device_addr);

/**
 * @brief Deinitialize SHT30 sensor
 * @param handle Pointer to SHT30 handle structure
 */
void sht30_deinit(sht30_handle_t *handle);

/**
 * @brief Read temperature and humidity
 * @param handle Pointer to SHT30 handle structure
 * @param data Pointer to data structure to fill
 * @return true if read successful, false otherwise
 */
bool sht30_read(sht30_handle_t *handle, sht30_data_t *data);

/**
 * @brief Check if hardware is present
 * @param handle Pointer to SHT30 handle structure
 * @return true if hardware detected, false if using synthetic data
 */
bool sht30_is_hardware_present(sht30_handle_t *handle);

#ifdef __cplusplus
}
#endif
