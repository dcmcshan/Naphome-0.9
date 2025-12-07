/**
 * @file bh1750_driver.h
 * @brief BH1750 Ambient Light Sensor Driver
 * 
 * I2C Address: 0x23 (ADDR pin LOW) or 0x5C (ADDR pin HIGH)
 * Supports synthetic data generation if hardware is not present
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BH1750_I2C_ADDR_LOW  0x23
#define BH1750_I2C_ADDR_HIGH 0x5C
#define BH1750_I2C_ADDR      BH1750_I2C_ADDR_LOW  // Default address

// BH1750 commands
#define BH1750_CMD_POWER_DOWN     0x00
#define BH1750_CMD_POWER_ON       0x01
#define BH1750_CMD_RESET          0x07
#define BH1750_CMD_CONT_H_MODE    0x10  // Continuous high resolution mode (1 lx, 120ms)
#define BH1750_CMD_CONT_H_MODE2   0x11  // Continuous high resolution mode 2 (0.5 lx, 120ms)
#define BH1750_CMD_CONT_L_MODE    0x13  // Continuous low resolution mode (4 lx, 16ms)
#define BH1750_CMD_ONE_H_MODE     0x20  // One-time high resolution mode (1 lx, 120ms)
#define BH1750_CMD_ONE_H_MODE2    0x21  // One-time high resolution mode 2 (0.5 lx, 120ms)
#define BH1750_CMD_ONE_L_MODE     0x23  // One-time low resolution mode (4 lx, 16ms)

#define BH1750_MEASURE_DELAY_MS   120   // Measurement delay for high resolution mode

typedef struct {
    float lux;              // Illuminance in lux
    bool valid;             // True if data is valid
    bool hardware_present;  // True if real hardware detected
} bh1750_data_t;

typedef struct {
    i2c_port_t i2c_port;
    uint8_t device_addr;
    bool initialized;
    bool hardware_present;
    // Synthetic data generation
    float synthetic_lux_base;
    uint32_t synthetic_counter;
} bh1750_handle_t;

/**
 * @brief Initialize BH1750 sensor
 * @param handle Pointer to BH1750 handle structure
 * @param i2c_port I2C port number (I2C_NUM_0 or I2C_NUM_1)
 * @param device_addr I2C device address (default: BH1750_I2C_ADDR)
 * @return true if initialized successfully, false otherwise
 */
bool bh1750_init(bh1750_handle_t *handle, i2c_port_t i2c_port, uint8_t device_addr);

/**
 * @brief Deinitialize BH1750 sensor
 * @param handle Pointer to BH1750 handle structure
 */
void bh1750_deinit(bh1750_handle_t *handle);

/**
 * @brief Read illuminance value
 * @param handle Pointer to BH1750 handle structure
 * @param data Pointer to data structure to fill
 * @return true if read successful, false otherwise
 */
bool bh1750_read(bh1750_handle_t *handle, bh1750_data_t *data);

/**
 * @brief Check if hardware is present
 * @param handle Pointer to BH1750 handle structure
 * @return true if hardware detected, false if using synthetic data
 */
bool bh1750_is_hardware_present(bh1750_handle_t *handle);

#ifdef __cplusplus
}
#endif
