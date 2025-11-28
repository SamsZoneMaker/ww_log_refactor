/**
 * @file no_macro_demo.c
 * @brief Demo: Using log system WITHOUT defining CURRENT_FILE_ID macro
 *
 * This file demonstrates the new compilation method where:
 * - NO #define CURRENT_FILE_ID needed
 * - NO #define CURRENT_MODULE_ID needed
 * - File ID and Module ID are injected at compile time via -D flags
 */

#include "ww_log.h"

// ❌ OLD WAY (not needed anymore!):
// #define CURRENT_FILE_ID   FILE_ID_DEMO_PROCESS
// #define CURRENT_MODULE_ID WW_LOG_MOD_DEMO

// ✅ NEW WAY: Just include ww_log.h and use the macros!

void no_macro_demo_function(void)
{
    int value = 42;

    // Use log macros directly - file ID and module ID are auto-injected!
    TEST_LOG_INF_MSG("Demo function started");
    TEST_LOG_DBG_MSG("Processing value: %d", value);

    if (value > 40) {
        TEST_LOG_WRN_MSG("Value is high: %d", value);
    }

    TEST_LOG_INF_MSG("Demo function completed");
}
