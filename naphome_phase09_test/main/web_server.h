/**
 * @file web_server.h
 * @brief HTTP Web Server with mDNS for Naphome Status Monitoring
 * 
 * Provides web interface at nap.local showing MCU usage, memory, tasks, etc.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration - test_status_t is defined in naphome_test_suite.c
// We'll use int for the web server interface to avoid enum conflicts
// The values match: 0=PASS, 1=WARNING, 2=FAIL, 3=NOT_IMPLEMENTED

/**
 * @brief Update test status for web display
 * @param test_num Test number (1-12)
 * @param status Test status (0=PASS, 1=WARNING, 2=FAIL, 3=NOT_IMPLEMENTED)
 * @param test_name Test name
 */
void web_server_update_test_status(int test_num, int status, const char *test_name);

/**
 * @brief Trigger the demo/test suite to run
 * @return true if demo was started, false if already running
 */
bool web_server_trigger_demo(void);

/**
 * @brief Initialize and start the web server with mDNS
 * @return ESP_OK on success
 */
esp_err_t web_server_start(void);

/**
 * @brief Stop the web server
 * @return ESP_OK on success
 */
esp_err_t web_server_stop(void);

#ifdef __cplusplus
}
#endif
