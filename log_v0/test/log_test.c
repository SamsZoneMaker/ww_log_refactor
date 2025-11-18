#include "../include/log_api.h"
#include "../include/log_filter.h"
#include "../include/log_storage.h"

// extern declarations
extern void uart_init();
extern void app_start();

int main() {
    // 测试ecode mode
    LOG_INFO(MODULE_APP, "Test %d", 123);

    // 调用不同文件
    uart_init();
    app_start();

    // 测试过滤
    log_level_mask = 1 << LOG_LEVEL_ERROR;  // only error
    LOG_INFO(MODULE_APP, "Filtered %d", 456);  // should be filtered
    LOG_ERROR(MODULE_APP, "Error %d", 789);  // should show

    // 测试存储
    log_flush_to_external();

    return 0;
}