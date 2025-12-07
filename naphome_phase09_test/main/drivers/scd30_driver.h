/**
 * @file scd30_driver.h
 * @brief SCD30 CO2, Temperature, and Humidity Sensor Driver
 * 
 * I2C Address: 0x61
 * Supports synthetic data generation if hardware is not present
 * Note: SCD30 uses CRC-8 checksum for data validation
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCD30_I2C_ADDR 0x61

// SCD30 commands (16-bit, big-endian)
#define SCD30_CMD_START_CONT_MEAS    0x0010  // Start continuous measurement
#define SCD30_CMD_STOP_MEAS          0x0104  // Stop continuous measurement
#define SCD30_CMD_SET_MEAS_INTERVAL 0x4600  // Set measurement interval
#define SCD30_CMD_GET_DATA_READY    0x0202  // Get data ready status
#define SCD30_CMD_READ_MEASUREMENT  0x0300  // Read measurement
#define SCD30_CMD_SET_TEMP_OFFSET   0x5403  // Set temperature offset
#define SCD30_CMD_SET_ALT_COMP      0x5102  // Set altitude compensation
#define SCD30_CMD_SOFT_RESET        0xD304  // Soft reset
#define SCD30_CMD_GET_FW_VERSION    0xD100  // Get firmware version
#define SCD30_CMD_SET_AUTO_CAL      0x5306  // Set automatic calibration

#define SCD30_MEASURE_DELAY_MS      2000    // Measurement delay (2 seconds for continuous mode)

typedef struct {
    float co2_ppm;          // CO2 concentration in ppm
    float temperature_c;    // Temperature in Celsius
    float humidity_rh;      // Relative humidity in %
    bool valid;             // True if data is valid
    bool hardware_present;  // True if real hardware detected
} scd30_data_t;

typedef struct {
    i2c_port_t i2c_port;
    uint8_t device_addr;
    bool initialized;
    bool hardware_present;
    // Synthetic data generation
    float synthetic_co2_base;
    float synthetic_temp_base;
    float synthetic_humidity_base;
    uint32_t synthetic_counter;
} scd30_handle_t;

/**
 * @brief Initialize SCD30 sensor
 * @param handle Pointer to SCD30 handle structure
 * @param i2c_port I2C port number (I2C_NUM_0 or I2C_NUM_1)
 * @param device_addr I2C device address (default: SCD30_I2C_ADDR)
 * @return true if initialized successfully, false otherwise
 */
bool scd30_init(scd30_handle_t *handle, i2c_port_t i2c_port, uint8_t device_addr);

/**
 * @brief Deinitialize SCD30 sensor
 * @param handle Pointer to SCD30 handle structure
 */
void scd30_deinit(scd30_handle_t *handle);

/**
 * @brief Read CO2, temperature, and humidity
 * @param handle Pointer to SCD30 handle structure
 * @param data Pointer to data structure to fill
 * @return true if read successful, false otherwise
 */
bool scd30_read(scd30_handle_t *handle, scd30_data_t *data);

/**
 * @brief Check if hardware is present
 * @param handle Pointer to SCD30 handle structure
 * @return true if hardware detected, false if using synthetic data
 */
bool scd30_is_hardware_present(scd30_handle_t *handle);

#ifdef __cplusplus
}
#endif
