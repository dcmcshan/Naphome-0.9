/**
 * @file sgp30_driver.h
 * @brief SGP30 VOC and eCO2 Sensor Driver
 * 
 * I2C Address: 0x58
 * Supports synthetic data generation if hardware is not present
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SGP30_I2C_ADDR 0x58
#define SGP30_CMD_INIT_AIR_QUALITY 0x2003
#define SGP30_CMD_MEASURE_AIR_QUALITY 0x2008
#define SGP30_CMD_GET_SERIAL_ID 0x3682
#define SGP30_MEASURE_DELAY_MS 12

typedef struct {
    uint16_t tvoc_ppb;      // TVOC in parts per billion
    uint16_t eco2_ppm;      // eCO2 in parts per million
    bool valid;             // True if data is valid
    bool hardware_present;  // True if real hardware detected
} sgp30_data_t;

typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t i2c_dev;
    uint8_t device_addr;
    bool initialized;
    bool hardware_present;
    // Synthetic data generation
    uint16_t synthetic_tvoc_base;
    uint16_t synthetic_eco2_base;
    uint32_t synthetic_counter;
} sgp30_handle_t;

/**
 * @brief Initialize SGP30 sensor
 */
bool sgp30_init(sgp30_handle_t *handle, i2c_master_bus_handle_t i2c_bus, uint8_t device_addr);

/**
 * @brief Deinitialize SGP30 sensor
 */
void sgp30_deinit(sgp30_handle_t *handle);

/**
 * @brief Read TVOC and eCO2 values
 */
bool sgp30_read(sgp30_handle_t *handle, sgp30_data_t *data);

/**
 * @brief Check if hardware is present
 */
bool sgp30_is_hardware_present(sgp30_handle_t *handle);

#ifdef __cplusplus
}
#endif
