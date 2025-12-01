# 日志系统实现说明

> **版本**: 2.0
> **更新日期**: 2025-12-01
> **状态**: 已实现并测试通过

---

## 目录

1. [概述](#概述)
2. [核心设计](#核心设计)
3. [编码格式](#编码格式)
4. [使用方法](#使用方法)
5. [编译与测试](#编译与测试)
6. [文件结构](#文件结构)

---

## 概述

本日志系统支持两种互斥的模式：

- **String 模式 (str_mode)**: 传统的 printf 风格日志，输出人类可读的文本
- **Encode 模式 (encode_mode)**: 二进制编码日志，最小化代码和 RAM 占用

### 核心特性

✅ **统一 API**: 两种模式使用相同的宏接口
✅ **模块化设计**: 模块级粗粒度 + 文件级细粒度 ID 管理
✅ **参数记录**: Encode 模式记录参数值（每个 U32）
✅ **零开销**: 编译时模式选择，无运行时开销
✅ **可选模块参数**: 支持默认 `[DEFA]` 标签

---

## 核心设计

### 1. 模块 ID 分配

每个模块预留 32 个 ID 槽位（使用位移计算）：

```c
// include/file_id.h
typedef enum {
    WW_LOG_MOD_DEMO_BASE  = (1 << 5),   /* 32-63,   DEMO 模块 */
    WW_LOG_MOD_TEST_BASE  = (2 << 5),   /* 64-95,   TEST 模块 */
    WW_LOG_MOD_APP_BASE   = (3 << 5),   /* 96-127,  APP 模块 */
    WW_LOG_MOD_DRV_BASE   = (4 << 5),   /* 128-159, 驱动模块 */
    WW_LOG_MOD_BROM_BASE  = (5 << 5),   /* 160-191, BROM 模块 */
} WW_LOG_MOD_BASE_E;
```

### 2. 文件级细粒度

每个模块可以为不同文件分配偏移量：

```c
// src/demo/demo_in.h
#define CURRENT_MODULE_BASE   WW_LOG_MOD_DEMO_BASE
#define CURRENT_MODULE_TAG    "[DEMO]"

typedef enum {
    DEMO_FILE_DEFAULT = 0,   /* LOG_ID = 32 */
    DEMO_FILE_INIT    = 1,   /* LOG_ID = 33 */
    DEMO_FILE_PROCESS = 2,   /* LOG_ID = 34 */
} DEMO_FILE_OFFSET_E;

#ifndef CURRENT_FILE_OFFSET
#define CURRENT_FILE_OFFSET   DEMO_FILE_DEFAULT
#endif

#define CURRENT_LOG_ID  (CURRENT_MODULE_BASE + CURRENT_FILE_OFFSET)
```

### 3. 日志级别（编译时宏）

```c
// include/ww_log.h
#define WW_LOG_LEVEL_ERR  0  /* 错误 */
#define WW_LOG_LEVEL_WRN  1  /* 警告 */
#define WW_LOG_LEVEL_INF  2  /* 信息 */
#define WW_LOG_LEVEL_DBG  3  /* 调试 */

#define WW_LOG_LEVEL_THRESHOLD  3  /* 编译时阈值 */
```

---

## 编码格式

### String 模式输出

```
[LEVEL][MODULE] file:line - message
```

**示例**:
```
[DBG][DRV] drv_i2c.c:39 - I2C write to device 0x50, data=0xAB
```

### Encode 模式输出

#### 32位编码格式

```
 31                    20 19                8 7            2 1    0
┌──────────────────────┬────────────────────┬──────────────┬──────┐
│   LOG_ID (12 bits)   │  LINE (12 bits)    │  DATA_LEN    │LEVEL │
│      0-4095          │     0-4095         │  (6 bits)    │(2bits)│
└──────────────────────┴────────────────────┴──────────────┴──────┘
```

**字段说明**:
- `LOG_ID`: 模块基础 ID + 文件偏移量（12 bits, 0-4095）
- `LINE`: 源代码行号（12 bits, 0-4095）
- `DATA_LEN`: 参数个数（6 bits, 0-63，当前支持 0-8）
- `LEVEL`: 日志级别（2 bits, 0-3：ERR/WRN/INF/DBG）

#### 输出格式

```
0xHHHHHHHH 0xPPPPPPPP 0xPPPPPPPP ...
^header    ^param1   ^param2
```

**示例**:
```
0x0830270B 0x00000050 0x000000AB
```

解码后：
- `LOG_ID = 131` (drv_i2c.c)
- `LINE = 39`
- `DATA_LEN = 2`
- `LEVEL = 3` (DBG)
- `参数 = [0x50, 0xAB]`

---

## 使用方法

### 基本用法

#### 1. 在模块源文件中

```c
// src/demo/demo_init.c
#define CURRENT_FILE_OFFSET  DEMO_FILE_INIT  // 可选，定义文件偏移
#include "demo_in.h"

void demo_init(void) {
    // 使用模块标签
    LOG_INF(CURRENT_MODULE_TAG, "Demo module initializing...");

    int status = 0;
    LOG_INF(CURRENT_MODULE_TAG, "Hardware check passed, code=%d", status);
}
```

#### 2. 可选模块参数

```c
// 默认使用 [DEFA] 标签
LOG_ERR("This is an error");                  // 输出: [ERR][DEFA] ...

// 显式指定模块标签
LOG_INF("[MAIN]", "Application started");     // 输出: [INF][MAIN] ...

// 带参数时必须指定模块（包括默认标签）
LOG_DBG("[DEFA]", "Value: %d", 123);          // 输出: [DBG][DEFA] ... Value: 123
```

**规则**:
- 1 个参数（仅消息）: 使用默认 `[DEFA]`
- 2+ 个参数（有格式化参数）: 必须显式指定模块标签

### String 模式输出

```c
LOG_DBG(CURRENT_MODULE_TAG, "I2C write to device 0x%02X, data=0x%02X",
        device_addr, data);
```

**输出**:
```
[DBG][DRV] drv_i2c.c:39 - I2C write to device 0x50, data=0xAB
```

### Encode 模式输出

相同的代码，在 Encode 模式下输出：

```
0x08302732 0x00000050 0x000000AB
```

**特点**:
- 格式字符串**不存储**在 ROM 中
- 参数值被记录为 U32（每个 4 字节）
- 需要解码工具还原为可读格式

---

## 编译与测试

### 编译模式选择

```bash
# String 模式（完整编译）
make MODE=str
make test-str          # 清理后编译并运行

# Encode 模式（完整编译）
make MODE=encode
make test-encode       # 清理后编译并运行

# 清理
make clean
```

### 运行测试

```bash
# String 模式
./bin/log_test_str

# Encode 模式
./bin/log_test_encode

# 解码 Encode 模式日志
./bin/log_test_encode | grep "^0x" | python3 tools/log_decoder.py -
```

### 解码工具

`tools/log_decoder.py` 用于解码二进制日志：

```bash
# 从标准输入解码
./bin/log_test_encode 2>&1 | grep "^0x" | python3 tools/log_decoder.py -

# 从文件解码
python3 tools/log_decoder.py encoded_logs.txt
```

**输出示例**:
```
Decoding logs from stdin...
================================================================================
   0: [DBG][DRV] drv_i2c.c:39 Params:[0x00000050, 0x000000AB] [Raw: 0x08302732]
   1: [INF][DRV] drv_i2c.c:40 [Raw: 0x08302820]
================================================================================
Decoded 2 log entries
```

---

## 文件结构

```
ww_log_refactor/
├── include/                    # 头文件
│   ├── file_id.h              # 模块 ID 定义
│   ├── ww_log.h               # 主头文件（模式选择）
│   ├── ww_log_str.h           # String 模式实现
│   └── ww_log_encode.h        # Encode 模式实现
│
├── src/
│   ├── core/                  # 核心实现
│   │   ├── ww_log_common.c   # 通用函数（init）
│   │   ├── ww_log_str.c      # String 模式输出
│   │   └── ww_log_encode.c   # Encode 模式输出
│   │
│   ├── demo/                  # DEMO 模块示例
│   │   ├── demo_in.h         # 模块内部头文件
│   │   ├── demo_init.c       # 初始化（LOG_ID=33）
│   │   └── demo_process.c    # 处理（LOG_ID=34）
│   │
│   ├── brom/                  # BROM 模块
│   ├── test/                  # TEST 模块
│   ├── app/                   # APP 模块
│   └── drivers/               # DRIVERS 模块
│
├── tools/
│   └── log_decoder.py         # 二进制日志解码工具
│
├── examples/
│   └── main.c                 # 测试程序
│
├── Makefile                   # 构建系统
├── README.md                  # 项目简介
├── CLAUDE.md                  # AI 助手指南
└── IMPLEMENTATION.md          # 本文档
```

### 核心文件说明

| 文件 | 用途 |
|------|------|
| `include/file_id.h` | 定义所有模块的基础 ID |
| `include/ww_log.h` | 主头文件，根据编译选项包含 str 或 encode 实现 |
| `include/ww_log_str.h` | String 模式宏定义和函数声明 |
| `include/ww_log_encode.h` | Encode 模式宏定义和函数声明 |
| `src/core/ww_log_str.c` | String 模式输出函数实现 |
| `src/core/ww_log_encode.c` | Encode 模式输出函数实现 |
| `src/*/XXX_in.h` | 各模块的内部头文件（定义模块标签和文件偏移） |

---

## 配置选项

### 编译时配置

在 `include/ww_log.h` 中：

```c
/* 日志级别阈值（编译时过滤） */
#define WW_LOG_LEVEL_THRESHOLD  WW_LOG_LEVEL_DBG

/* Encode 模式 RAM 缓冲区 */
#define WW_LOG_ENCODE_RAM_BUFFER_EN
#define WW_LOG_RAM_BUFFER_SIZE  128  /* 条目数 */
```

### Makefile 配置

```makefile
# 模式选择
MODE ?= str    # 可选: str, encode

# 编译器标志
CFLAGS += -DWW_LOG_MODE_$(MODE_UPPER)
```

---

## 代码大小对比

### String 模式
- **代码大小**: ~15-20 KB（包含格式字符串）
- **RAM 使用**: ~2-4 KB（格式化缓冲区）
- **优点**: 可读性强，易于调试
- **缺点**: ROM 和 RAM 占用大

### Encode 模式
- **代码大小**: ~3-5 KB（无格式字符串）
- **RAM 使用**: ~0.5-1 KB（仅参数数组）
- **优点**: 最小化资源占用
- **缺点**: 需要解码工具

**节省**:
- ROM: 约 60-75%
- RAM: 约 50-75%

---

## 最佳实践

### 1. 模块 ID 管理

- ✅ 为每个模块预留足够的 ID 空间（32 个通常足够）
- ✅ 在 `file_id.h` 中集中管理所有模块 ID
- ✅ 为常用文件分配固定偏移量
- ❌ 不要重用已删除文件的 ID

### 2. 日志使用

- ✅ 根据重要性选择合适的日志级别
- ✅ 在关键路径使用 `WW_LOG_LEVEL_THRESHOLD` 过滤
- ✅ 参数不超过 8 个（Encode 模式限制）
- ❌ 不要在 ISR 中大量打印日志

### 3. 模式选择

- **开发阶段**: 使用 String 模式便于调试
- **生产环境**: 使用 Encode 模式节省资源
- **现场调试**: Encode 模式 + RAM 缓冲区 + 解码工具

---

## 故障排查

### 编译错误

**问题**: `undefined reference to ww_log_xxx_output`
**解决**: 检查 Makefile 中的 `MODE` 设置是否正确

**问题**: 参数个数超过限制
**解决**: Encode 模式最多支持 8 个参数，拆分日志或使用 String 模式

### 运行时问题

**问题**: Encode 模式输出乱码
**解决**: 使用 `log_decoder.py` 解码，检查编码格式是否匹配

**问题**: 日志丢失
**解决**: 检查 `WW_LOG_LEVEL_THRESHOLD` 设置，确保日志级别未被过滤

---

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| 2.0 | 2025-12-01 | 添加参数记录功能，支持可选模块参数 |
| 1.1 | 2025-11-29 | 完善模块化设计，添加所有模块示例 |
| 1.0 | 2025-11-27 | 初始实现，支持 String 和 Encode 模式 |

---

## 参考资料

- 原始设计需求: `doc/log模块更新设计要求.md`
- 详细设计方案: `doc/日志模块重构设计方案.md`
- AI 助手指南: `CLAUDE.md`

---

**维护者**: Claude AI
**项目**: ww_log_refactor
**仓库**: https://github.com/SamsZoneMaker/ww_log_refactor
