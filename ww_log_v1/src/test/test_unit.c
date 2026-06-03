/**
 * @file test_unit.c
 * @brief TEST module unit-test stub.
 *
 * The TEST module is disabled in log_config.json, so every LOG below is
 * compiled out (zero code size) regardless of mode. The function still runs.
 */

#include "test_unit.h"
#include <stdio.h>

void test_unit_run(void)
{
    LOG_INF("Unit tests starting (this log is statically disabled)");
    LOG_DBG("case 1 value=%d", 42);
    LOG_ERR("this error is also compiled out");
    printf("    (test_unit_run executed; its LOG calls are statically disabled)\n");
}
