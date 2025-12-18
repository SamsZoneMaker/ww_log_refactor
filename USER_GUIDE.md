# WW日志系统用户指南

**版本：** 2.0
**更新日期：** 2025-12-18

## 快速开始

### 1. 配置日志模式

编辑 [`include/ww_log.h`](include/ww_log.h:63)，取消注释所需模式：

```c
/* 选择一种模式（只能启用一个）： */
// #define WW_LOG_MODE_STR       // String模式 - printf风格，人类可读
#define WW_LOG_MODE_ENCODE      // Encode模式 - 二进制编码，最小体积
// #define WW_LOG_MODE_DISABLED  // 禁用所有日志
```

### 2. 编译项目

```bash
make clean
make
```

### 3. 运行程序

```bash
make run
```

### 4. 切换模式

1. 编辑 [`include/ww_log.h`](include/ww_log.h:63) 更改模式定义
2. 重新编译：`make clean && make`

---

## 基本使用

### 日志API

系统提供4个日志级别宏：

```c
LOG_ERR(fmt, ...)  // 错误 - 系统故障，关键问题
LOG_WRN(fmt, ...)  // 警告 - 潜在问题
LOG_INF(fmt, ...)  // 信息 - 重要状态变化
LOG_DBG(fmt, ...)  // 调试 - 详细执行流程
```

### 使用示例

```c
#include "ww_log.h"

void my_function(void) {
    int value = 42;

    // 无参数
    LOG_INF("Function started");

    // 单个参数
    LOG_DBG("Processing value: %d", value);

    // 多个参数
    LOG_INF("Result: x=%d, y=%d", 10, 20);

    // 错误日志
    if (value < 0) {
        LOG_ERR("Invalid value: %d", value);
    }
}
```

### 输出示例

**String模式：**
```
[INF] my_file.c:15 - Function started
[DBG] my_file.c:18 - Processing value: 42
[INF] my_file.c:21 - Result: x=10, y=20
```

**Encode模式：**
```
0x0C300F02
0x0C301203 0x0000002A
0x0C301502 0x0000000A 0x00000014
```

---

## 添加新文件

### 步骤1：在配置文件中注册

编辑 [`log_config.json`](log_config.json)：

```json
{
  "files": {
    "src/my_module/my_file.c": {
      "module": "APP",
      "offset": 3,
      "description": "My new file"
    }
  }
}
```

### 步骤2：在代码中使用

```c
// src/my_module/my_file.c
#include "ww_log.h"

void my_function(void) {
    LOG_INF("Hello from my new file!");
}
```

### 步骤3：重新编译

```bash
make clean
make
```

**就这么简单！** 不需要在代码中定义任何宏。

---

## 模块控制

### 静态开关（编译时）

**完全移除代码，零体积影响**

禁用单个模块：
```bash
make STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0"
```

禁用多个模块：
```bash
make STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0 -DWW_LOG_STATIC_MODULE_TEST_EN=0"
```

**效果：** 被禁用模块的所有LOG调用在预处理阶段被替换为空语句，编译器优化后完全不占用代码空间。

### 动态开关（配置文件）

编辑 [`log_config.json`](log_config.json)：

```json
{
  "modules": {
    "DEMO": {
      "id": 1,
      "enable": false,  // 禁用DEMO模块
      "base_id": 64
    }
  }
}
```

重新编译：
```bash
make clean
make
```

**效果：** 该模块的文件不会生成FILE_ID，日志宏变为空操作。

---

## 日志级别控制

### 编译时级别过滤

在 [`include/ww_log.h`](include/ww_log.h:50) 中设置：

```c
#define WW_LOG_COMPILE_THRESHOLD  WW_LOG_LEVEL_INF
```

或通过Makefile：
```bash
make CFLAGS="-DWW_LOG_COMPILE_THRESHOLD=2"
```

**级别值：**
- `0` (ERR) - 只编译错误日志
- `1` (WRN) - 编译错误和警告
- `2` (INF) - 编译错误、警告和信息
- `3` (DBG) - 编译所有日志（默认）

### 运行时级别过滤（String模式）

```c
#include "ww_log_modules.h"

// 设置全局日志级别
ww_log_set_level_threshold(WW_LOG_LEVEL_INF);  // 只输出INF及以下级别
```

---

## 模块管理

### 查看模块状态

```c
#include "ww_log_modules.h"

// 检查模块是否启用
if (ww_log_module_is_enabled(WW_LOG_MODULE_APP)) {
    // 模块已启用
}
```

### 运行时启用/禁用模块（String模式）

```c
// 禁用DEMO模块
ww_log_module_disable(WW_LOG_MODULE_DEMO);

// 启用DEMO模块
ww_log_module_enable(WW_LOG_MODULE_DEMO);

// 禁用所有模块
ww_log_module_disable_all();

// 启用所有模块
ww_log_module_enable_all();
```

---

## Encode模式解码

### 使用解码工具

```bash
# 运行程序并解码输出
./bin/log_test | grep "^0x" | python3 tools/log_decoder.py -
```

### 解码输出示例

```
[DBG][DRV] drv_i2c.c:39 Params:[0x00000050, 0x000000AB]
[INF][APP] app_main.c:42 Params:[]
[ERR][BROM] brom_boot.c:15 Params:[0x00000001]
```

---

## 常见问题

### Q1: 如何查看某个文件的ID？

**A:** 查看生成的 [`include/auto_file_ids.h`](include/auto_file_ids.h) 文件：

```c
#define FILE_ID_SRC_APP_APP_MAIN_C  193  /* Application main */
```

### Q2: 编译时提示找不到CURRENT_FILE_ID？

**A:** 确保文件已在 [`log_config.json`](log_config.json) 中注册，然后运行：
```bash
make clean
make
```

### Q3: 如何临时禁用某个文件的日志？

**A:** 从 [`log_config.json`](log_config.json) 中删除该文件的配置项，或将其模块设置为 `"enable": false`。

### Q4: 静态开关和动态开关有什么区别？

**A:**
- **静态开关**：编译时完全移除代码，零体积影响，需要重新编译
- **动态开关**：运行时控制输出，代码仍在二进制中，可动态切换

### Q5: 如何减少代码体积？

**A:** 使用以下方法：
1. 使用Encode模式而不是String模式
2. 使用静态开关禁用不需要的模块
3. 降低编译时日志级别阈值
4. 在生产环境使用 `WW_LOG_MODE_DISABLED`

---

## 最佳实践

### 1. 模块划分

按功能模块组织代码，每个模块预留足够的ID空间（64个文件）：

```json
{
  "modules": {
    "MY_MODULE": {
      "id": 6,
      "enable": true,
      "base_id": 384,  // 6 << 6
      "description": "My custom module"
    }
  }
}
```

### 2. 日志级别使用

- **ERR**: 系统故障、无法恢复的错误
- **WRN**: 可能导致问题的情况、资源不足
- **INF**: 重要的状态变化、关键操作
- **DBG**: 详细的执行流程、调试信息

### 3. 格式字符串

```c
// ✅ 好的做法
LOG_INF("User login: id=%d, name=%s", user_id, user_name);

// ❌ 避免
LOG_INF("User login");  // 缺少上下文信息
LOG_DBG("x=%d y=%d z=%d w=%d ...", ...);  // 参数过多（最多16个）
```

### 4. 开发vs生产

**开发环境：**
```c
#define WW_LOG_MODE_STR  // 人类可读
#define WW_LOG_COMPILE_THRESHOLD  WW_LOG_LEVEL_DBG  // 所有日志
```

**生产环境：**
```c
#define WW_LOG_MODE_ENCODE  // 最小体积
#define WW_LOG_COMPILE_THRESHOLD  WW_LOG_LEVEL_INF  // 只保留重要日志
```

或完全禁用：
```c
#define WW_LOG_MODE_DISABLED
```

### 5. 性能考虑

- Encode模式比String模式快约2-3倍
- 静态禁用的模块完全不影响性能
- 运行时过滤有轻微性能开销（检查模块掩码）

---

## 配置参考

### log_config.json结构

```json
{
  "modules": {
    "MODULE_NAME": {
      "id": 0,                  // 模块ID (0-31)
      "enable": true,           // 是否启用
      "base_id": 0,             // 基础ID (id << 6)
      "description": "描述"
    }
  },
  "files": {
    "path/to/file.c": {
      "module": "MODULE_NAME",  // 所属模块
      "offset": 0,              // 模块内偏移 (0-63)
      "description": "描述"
    }
  }
}
```

### 文件ID计算

```
最终文件ID = module.base_id + file.offset

例如：
- APP模块：base_id = 192 (3 << 6)
- app_main.c：offset = 1
- 最终ID = 192 + 1 = 193
```

---

## 故障排除

### 编译错误

**错误：** `No log mode defined!`
**解决：** 在 [`include/ww_log.h`](include/ww_log.h) 中取消注释一个模式定义

**错误：** `auto_file_ids.h: No such file`
**解决：** 运行 `make gen-log-ids` 生成文件

### 运行时问题

**问题：** 日志没有输出
**检查：**
1. 模块是否启用（静态和动态）
2. 日志级别是否正确
3. 文件是否在 `log_config.json` 中注册

**问题：** Encode模式输出乱码
**解决：** 使用 `tools/log_decoder.py` 解码

---

## 总结

WW日志系统提供了：

✅ **双模式支持** - String（调试）/ Encode（生产）
✅ **统一API** - 两种模式使用相同的宏
✅ **零配置** - 文件ID自动管理
✅ **灵活控制** - 静态+动态双重开关
✅ **高性能** - 最小化运行时开销
✅ **小体积** - Encode模式减少60-75%代码

**开始使用只需三步：**
1. 在 `ww_log.h` 中选择模式
2. 在代码中使用 `LOG_XXX()` 宏
3. 运行 `make` 编译

就这么简单！
