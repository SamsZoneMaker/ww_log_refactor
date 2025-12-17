/**
 * @file module_mask_demo.c
 * @brief 模块动态开关使用示例
 * @date 2025-12-01
 *
 * 本文件演示如何使用 ww_log_set_module_mask 及相关API来动态控制
 * 不同模块的日志输出，适用于调试、性能优化和故障诊断场景。
 */

#include "ww_log.h"
#include "ww_log_modules.h"
#include <stdio.h>

/* ========== 使用场景1: 初始化时的配置 ========== */

/**
 * @brief 场景1A: 开发环境 - 启用所有模块
 */
void scenario_development_mode(void)
{
    printf("\n=== 场景1A: 开发环境配置 ===\n");

    /* 启用所有模块，获取完整的日志信息 */
    ww_log_set_module_mask(0xFFFFFFFF);

    printf("已启用所有模块 (0xFFFFFFFF)\n");
    printf("当前掩码: 0x%08X\n", ww_log_get_module_mask());

    /* 测试输出 */
    LOG_INF(WW_LOG_MODULE_DEMO, "DEMO模块日志 - 会显示");
    LOG_INF(WW_LOG_MODULE_TEST, "TEST模块日志 - 会显示");
    LOG_INF(WW_LOG_MODULE_APP, "APP模块日志 - 会显示");
}

/**
 * @brief 场景1B: 生产环境 - 只启用关键模块
 */
void scenario_production_mode(void)
{
    printf("\n=== 场景1B: 生产环境配置 ===\n");

    /* 只启用 APP 和 DRIVERS 模块，减少日志噪音 */
    U32 production_mask = (1 << WW_LOG_MODULE_APP) |
                          (1 << WW_LOG_MODULE_DRIVERS);

    ww_log_set_module_mask(production_mask);

    printf("已启用关键模块 APP(3) 和 DRIVERS(4)\n");
    printf("当前掩码: 0x%08X\n", ww_log_get_module_mask());

    /* 测试输出 */
    LOG_INF(WW_LOG_MODULE_APP, "APP模块日志 - 会显示");
    LOG_INF(WW_LOG_MODULE_DRIVERS, "DRIVERS模块日志 - 会显示");
    LOG_INF(WW_LOG_MODULE_TEST, "TEST模块日志 - 不会显示");
}

/**
 * @brief 场景1C: 性能模式 - 禁用所有日志
 */
void scenario_performance_mode(void)
{
    printf("\n=== 场景1C: 性能模式配置 ===\n");

    /* 禁用所有模块，追求极致性能 */
    ww_log_set_module_mask(0x00000000);

    printf("已禁用所有模块 (0x00000000)\n");
    printf("当前掩码: 0x%08X\n", ww_log_get_module_mask());

    /* 测试输出 - 不会有任何日志 */
    LOG_INF(WW_LOG_MODULE_DEFAULT, "这条日志不会显示");
    printf("（上面的日志已被过滤）\n");
}

/* ========== 使用场景2: 动态调试 ========== */

/**
 * @brief 场景2A: 临时启用某个模块进行调试
 */
void scenario_debug_specific_module(void)
{
    printf("\n=== 场景2A: 调试特定模块 ===\n");

    /* 保存当前配置 */
    U32 saved_mask = ww_log_get_module_mask();
    printf("保存当前掩码: 0x%08X\n", saved_mask);

    /* 临时只启用 BROM 模块 */
    ww_log_set_module_mask(1 << WW_LOG_MODULE_BROM);
    printf("临时只启用 BROM 模块进行调试\n");

    /* 执行需要调试的代码 */
    LOG_INF(WW_LOG_MODULE_BROM, "BROM: 启动序列开始");
    LOG_DBG(WW_LOG_MODULE_BROM, "BROM: 检查启动参数");
    LOG_INF(WW_LOG_MODULE_APP, "APP: 这条日志被过滤");

    /* 恢复原来的配置 */
    ww_log_set_module_mask(saved_mask);
    printf("已恢复原配置: 0x%08X\n", ww_log_get_module_mask());
}

/**
 * @brief 场景2B: 逐步启用模块定位问题
 */
void scenario_progressive_debugging(void)
{
    printf("\n=== 场景2B: 渐进式调试 ===\n");

    /* 步骤1: 从最小日志开始 */
    ww_log_set_module_mask(1 << WW_LOG_MODULE_DEFAULT);
    printf("步骤1: 只启用 DEFAULT 模块\n");
    LOG_INF(WW_LOG_MODULE_DEFAULT, "基础系统日志");

    /* 步骤2: 添加 APP 模块 */
    ww_log_enable_module(WW_LOG_MODULE_APP);
    printf("步骤2: 添加 APP 模块 (掩码: 0x%08X)\n", ww_log_get_module_mask());
    LOG_INF(WW_LOG_MODULE_APP, "应用层日志");

    /* 步骤3: 添加 DRIVERS 模块 */
    ww_log_enable_module(WW_LOG_MODULE_DRIVERS);
    printf("步骤3: 添加 DRIVERS 模块 (掩码: 0x%08X)\n", ww_log_get_module_mask());
    LOG_INF(WW_LOG_MODULE_DRIVERS, "驱动层日志");
}

/* ========== 使用场景3: 故障诊断 ========== */

/**
 * @brief 场景3A: 检测到错误时启用详细日志
 */
void scenario_error_diagnostics(void)
{
    printf("\n=== 场景3A: 错误诊断 ===\n");

    /* 正常运行时使用精简日志 */
    ww_log_set_module_mask(0x00000009);  // 只启用 DEFAULT(0) 和 APP(3)

    /* 模拟检测到错误 */
    int error_detected = 1;

    if (error_detected) {
        printf("检测到错误！启用所有模块进行诊断...\n");
        ww_log_set_module_mask(0xFFFFFFFF);

        /* 收集详细信息 */
        LOG_ERR(WW_LOG_MODULE_DEFAULT, "系统错误发生");
        LOG_DBG(WW_LOG_MODULE_APP, "APP状态: running");
        LOG_DBG(WW_LOG_MODULE_DRIVERS, "DRIVERS状态: active");
        LOG_DBG(WW_LOG_MODULE_BROM, "BROM版本: 1.0");

        /* 可以在这里保存日志或触发上报 */
    }
}

/**
 * @brief 场景3B: 排除噪音大的模块
 */
void scenario_filter_noisy_module(void)
{
    printf("\n=== 场景3B: 过滤噪音模块 ===\n");

    /* 全部启用 */
    ww_log_set_module_mask(0xFFFFFFFF);

    /* 发现 TEST 模块日志太多，影响分析 */
    printf("TEST 模块日志太多，临时禁用...\n");
    ww_log_disable_module(WW_LOG_MODULE_TEST);

    printf("当前掩码: 0x%08X\n", ww_log_get_module_mask());

    /* 测试输出 */
    LOG_INF(WW_LOG_MODULE_APP, "APP日志 - 会显示");
    LOG_INF(WW_LOG_MODULE_TEST, "TEST日志 - 被过滤");
    LOG_INF(WW_LOG_MODULE_DRIVERS, "DRIVERS日志 - 会显示");
}

/* ========== 使用场景4: 条件日志 ========== */

/**
 * @brief 场景4: 根据运行状态动态调整
 */
void scenario_adaptive_logging(void)
{
    printf("\n=== 场景4: 自适应日志 ===\n");

    /* 模拟不同的系统状态 */
    typedef enum {
        SYS_STATE_BOOT,
        SYS_STATE_NORMAL,
        SYS_STATE_HEAVY_LOAD,
    } sys_state_t;

    sys_state_t state = SYS_STATE_BOOT;

    switch (state) {
        case SYS_STATE_BOOT:
            /* 启动阶段：启用所有日志 */
            printf("启动阶段：启用所有模块\n");
            ww_log_set_module_mask(0xFFFFFFFF);
            break;

        case SYS_STATE_NORMAL:
            /* 正常运行：精简日志 */
            printf("正常运行：精简日志\n");
            ww_log_set_module_mask(0x00000019);  // DEFAULT, APP, DRIVERS
            break;

        case SYS_STATE_HEAVY_LOAD:
            /* 高负载：只保留错误日志的模块 */
            printf("高负载：最小化日志\n");
            ww_log_set_module_mask(0x00000001);  // 只保留 DEFAULT
            break;
    }

    printf("当前掩码: 0x%08X\n", ww_log_get_module_mask());
}

/* ========== 使用场景5: 模块状态查询 ========== */

/**
 * @brief 场景5: 检查和报告模块状态
 */
void scenario_module_status_report(void)
{
    printf("\n=== 场景5: 模块状态报告 ===\n");

    /* 设置一个示例掩码 */
    ww_log_set_module_mask(0x0000001F);  // 启用模块 0-4

    printf("当前模块启用状态:\n");
    printf("  DEFAULT (0): %s\n", ww_log_is_module_enabled(WW_LOG_MODULE_DEFAULT) ? "ON" : "OFF");
    printf("  DEMO    (1): %s\n", ww_log_is_module_enabled(WW_LOG_MODULE_DEMO) ? "ON" : "OFF");
    printf("  TEST    (2): %s\n", ww_log_is_module_enabled(WW_LOG_MODULE_TEST) ? "ON" : "OFF");
    printf("  APP     (3): %s\n", ww_log_is_module_enabled(WW_LOG_MODULE_APP) ? "ON" : "OFF");
    printf("  DRIVERS (4): %s\n", ww_log_is_module_enabled(WW_LOG_MODULE_DRIVERS) ? "ON" : "OFF");
    printf("  BROM    (5): %s\n", ww_log_is_module_enabled(WW_LOG_MODULE_BROM) ? "ON" : "OFF");
}

/* ========== 主函数：运行所有示例 ========== */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║   模块动态开关 (ww_log_set_module_mask) 使用示例      ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n");

    /* 初始化日志系统 */
    ww_log_init();

    /* 运行各种场景 */
    scenario_development_mode();
    scenario_production_mode();
    scenario_performance_mode();
    scenario_debug_specific_module();
    scenario_progressive_debugging();
    scenario_error_diagnostics();
    scenario_filter_noisy_module();
    scenario_adaptive_logging();
    scenario_module_status_report();

    printf("\n=== 所有示例运行完成 ===\n");
    printf("\n总结:\n");
    printf("  1. 初始化时配置: 根据环境选择合适的模块掩码\n");
    printf("  2. 动态调试: 临时启用/禁用特定模块\n");
    printf("  3. 故障诊断: 错误时启用详细日志\n");
    printf("  4. 自适应: 根据系统状态调整日志级别\n");
    printf("  5. 状态查询: 检查模块启用状态\n");

    return 0;
}
