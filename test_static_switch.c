/**
 * @file test_static_switch.c
 * @brief Test file to verify static compile-time log filtering
 * @date 2025-12-09
 */

#include "include/ww_log.h"

/* Define CURRENT_LOG_ID for encode mode testing */
#define CURRENT_LOG_ID  FILE_ID_DEFAULT

void test_function(void) {
    // These should be compiled in or out based on WW_LOG_COMPILE_THRESHOLD

    // For encode mode: LOG_ERR(tag, fmt, ...)
    // For string mode: LOG_ERR(module_id, fmt, ...)

    #ifdef WW_LOG_MODE_STR
        // String mode uses module_id as first parameter
        LOG_ERR(0, "Error message - should always be compiled if threshold >= 0");
        LOG_WRN(0, "Warning message - should be compiled if threshold >= 1");
        LOG_INF(0, "Info message - should be compiled if threshold >= 2");
        LOG_DBG(0, "Debug message - should be compiled if threshold >= 3");

        // Test with parameters
        int value = 42;
        LOG_ERR(0, "Error with value: %d", value);
        LOG_WRN(0, "Warning with value: %d", value);
        LOG_INF(0, "Info with value: %d", value);
        LOG_DBG(0, "Debug with value: %d", value);
    #else
        // Encode mode uses tag as first parameter
        LOG_ERR("test", "Error message - should always be compiled if threshold >= 0");
        LOG_WRN("test", "Warning message - should be compiled if threshold >= 1");
        LOG_INF("test", "Info message - should be compiled if threshold >= 2");
        LOG_DBG("test", "Debug message - should be compiled if threshold >= 3");

        // Test with parameters
        int value = 42;
        LOG_ERR("test", "Error with value: %d", value);
        LOG_WRN("test", "Warning with value: %d", value);
        LOG_INF("test", "Info with value: %d", value);
        LOG_DBG("test", "Debug with value: %d", value);
    #endif
}

int main(void) {
    test_function();
    return 0;
}
