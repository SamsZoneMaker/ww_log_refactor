# ww_log v1 — 嵌入式日志系统

> 面向 ROM/RAM 严格受限固件的日志系统。核心思路：**设备只存 ID + 参数，格式串留在 PC**，大幅节省固件体积，同时保留调试友好的 string 模式和全关模式，三种模式编译期切换、调用点写法完全一致。

---

## 目录

1. [背景与设计目标](#1-背景与设计目标)
2. [三种编译模式](#2-三种编译模式)
3. [encode 编码格式](#3-encode-编码格式)
4. [ID 管理与映射文件](#4-id-管理与映射文件)
5. [日志开关](#5-日志开关)
6. [输出后端](#6-输出后端)
7. [panic 模式](#7-panic-模式)
8. [目录结构与文件说明](#8-目录结构与文件说明)
9. [快速开始](#9-快速开始)
10. [encode 解码闭环](#10-encode-解码闭环)
11. [真机集成指南](#11-真机集成指南)
12. [与 v0 的主要差异](#12-与-v0-的主要差异)

---

## 1. 背景与设计目标

传统 printf 风格日志（string 模式）把格式串、文件名等字面量全部编进固件，ROM 占用高；
在 4KB RAM 维护区里存文本也很快溢出。

**ww_log v1** 的解法：

- 调用点只产生一条**定长二进制 entry**（4 字节头 + N 个 U32 参数），格式串不进固件。
- entry 可直接写进掉电不丢的 **RAM 维护区（4 KB）**，再按需 flush 到外存（EEPROM/Flash）。
- PC 端用一份**映射文件**（`ww_log_map.json`）把二进制还原为可读日志。
- 三种模式（encode / string / disabled）**调用点写法完全一致**，切模式只改一行宏，不动业务代码。

---

## 2. 三种编译模式

在 `include/ww_log.h` 里三选一：

```c
#define WW_LOG_MODE_ENCODE     // 生产：定长二进制，省体积
// #define WW_LOG_MODE_STR     // 调试：printf 风格，直观
// #define WW_LOG_MODE_DISABLED // 全关：所有 LOG 宏展开为空，零体积
```

调用点写法在三种模式下**完全一致**：

```c
LOG_ERR("初始化失败，code=%d", err);
LOG_WRN("队列将满，used=%d", q->used);
LOG_INF("启动完成，version=%d.%d", major, minor);
LOG_DBG("任务入队，id=%d", task_id);
```

三种模式的实际输出对比：

| 模式 | 同一行代码的输出 |
|------|----------------|
| `STR` | `[INF] demo_init.c:17 - Hardware check passed, code=0` |
| `ENCODE` | `0x08000441 0x00000000`（U32 头 + 参数的 hex 帧） |
| `DISABLED` | 无任何输出，宏展开为空语句，零 ROM 占用 |

---

## 3. encode 编码格式

### 3.1 U32 头结构

每条日志的头部是一个 **32 位整数**，布局如下：

```
 31                20 19              6 5         0
┌────────────────────┬──────────────────┬───────────┐
│   file_id (12 位)  │   line (14 位)   │ param_cnt │
│                    │                  │  (6 位)   │
└────────────────────┴──────────────────┴───────────┘
         │
         └─ file_id = [ module_id : 高 5 位 ][ offset : 低 7 位 ]
```

| 字段 | 位宽 | 范围 | 含义 |
|------|------|------|------|
| `file_id` | 12 | 0–4095 | 文件唯一 ID；高 5 位 = 模块 ID（0–31），低 7 位 = 模块内文件序号（0–127） |
| `line` | 14 | 0–16383 | `__LINE__` 行号 |
| `param_count` | 6 | 0–63 | 紧跟在头之后的 U32 参数个数 |

U32 头之后紧跟 `param_count` 个 U32 参数。一条完整 entry 占 `4 + param_count × 4` 字节，**自描述，无需帧边界标记**。

### 3.2 level 不进编码

level 只在**运行期过滤**时使用（`level > 阈值` 直接 return），不存入 entry。
decode 时由 map 文件按 `(file_id, line)` 还原 level。
这样节省出 2 位，使 `line` 从 v0 的 12 位（上限 4095）扩展到 14 位（上限 16383）。

### 3.3 手算示例

```
0x08000441  0x00000000
│
├─ bits[31:20] = 0x080 = 128  →  file_id=128
├─ bits[19:6]  = 0x011 = 17   →  line=17
└─ bits[5:0]   = 0x01  = 1    →  param_count=1，参数 = 0x00000000 = 0

查 ww_log_map.json：file_id=128, line=17
→ fmt = "Hardware check passed, code=%d"
→ 最终：[INF] demo_init.c:17 - Hardware check passed, code=0
```

### 3.4 `%s` 的处理

`%s` 对应的指针值照常作为 U32 参数存储（运行期无额外开销），但字符串内容无法还原。
decode 输出占位：`<%s@0xXXXXXXXX>`。
构建期扫描器遇到 `%s` 发出告警，不阻断编译。

---

## 4. ID 管理与映射文件

### 4.1 输入：`log_config.json`（人工维护）

只需登记**模块名 → 源码目录**的映射，模块 ID 由开发者分配一次后固定不变：

```json
{
  "modules": {
    "DEMO":    { "id": 1, "dirs": ["src/demo"],               "enable": true  },
    "TEST":    { "id": 2, "dirs": ["src/test"],               "enable": false },
    "DRIVERS": { "id": 4, "dirs": ["src/drivers", "src/hal"], "enable": true  }
  },
  "unregistered": "warn"
}
```

- `id`：模块 ID，范围 0–31，**一旦分配不可更改**（它是运行期动态开关的 key）。
- `dirs`：该模块下的源码目录（路径相对于 `ww_log_v1/`，支持多目录）；匹配时取**最长前缀优先**。
- `enable`：`false` 时该模块下所有文件的 LOG 宏在编译期展开为空语句，零体积。
- 不在任何 `dirs` 下的 `.c` 文件：发出告警，该文件日志静默关闭，不阻断编译。

### 4.2 生成：`ww_log_map.json`（构建+解码共用）

运行 `scripts/log/gen_log_map.py` 扫描所有 `.c`，提取每条 `LOG_xxx(...)` 的行号、level、格式串，生成：

```json
{
  "meta": {
    "version": "",
    "build_time": "2026-06-05T10:19:52Z",
    "encoding": "file12_line14_pcnt6"
  },
  "modules": {
    "1": { "name": "DEMO",    "enable": true  },
    "2": { "name": "TEST",    "enable": false },
    "4": { "name": "DRIVERS", "enable": true  }
  },
  "files": {
    "128": { "path": "src/demo/demo_init.c",    "module": "DEMO",    "present": true },
    "129": { "path": "src/demo/demo_process.c", "module": "DEMO",    "present": true },
    "512": { "path": "src/drivers/drv_uart.c",  "module": "DRIVERS", "present": true }
  },
  "entries": [
    { "file_id": 128, "line": 10, "level": "INF", "fmt": "Demo module initializing..."      },
    { "file_id": 128, "line": 17, "level": "INF", "fmt": "Hardware check passed, code=%d"  },
    { "file_id": 512, "line": 8,  "level": "INF", "fmt": "UART init, baud=%d"              }
  ]
}
```

该文件**一文件两用**：

| 用途 | 命令 / 方式 |
|------|------------|
| 生成构建产物 | `gen_log_map.py log_config.json --makefile` → `output/file_ids.mk` |
| 生成 C 宏头文件 | `gen_log_map.py log_config.json --header` → `output/auto_file_ids.h` |
| decode 还原日志 | `log_decoder.py --map ww_log_map.json <input>` |

### 4.3 file_id 锁定（关键）

`file_id = module_id × 128 + offset`（offset 为该文件在模块内的序号）。

脚本**每次重新生成前先读旧 `ww_log_map.json`**：
- 已存在的文件 → 保持原 offset **不变**。
- 新增文件 → 分配最低空闲 offset。
- 已删除的文件 → offset **保留占位、不回收**（`present: false`）。

意义：旧固件生成的历史日志，即使当前代码库已增删文件，仍能被新 map 正确 decode。

### 4.4 构建期参数校验

扫描时统计格式串中 `%` 占位符个数（排除 `%%`），与实际传参数量比对，不一致则**发出告警**（不阻断构建）。

---

## 5. 日志开关

### 5.1 静态开关（编译期，零代码）

Makefile 为每个 `.c` 文件注入三个宏：

| 宏 | 含义 |
|----|------|
| `CURRENT_FILE_ID` | 该文件的 file_id（由 `file_ids.mk` 查表注入） |
| `CURRENT_MODULE_ID` | 该文件所属模块的 module_id |
| `CURRENT_MODULE_STATIC_EN` | 该模块的 `enable`（0 或 1） |

当 `CURRENT_MODULE_STATIC_EN = 0` 时，LOG 宏通过 token 拼接技巧展开为空语句（`do{}while(0)`），**该文件不产生任何日志相关代码和字符串**。

另有编译期 level 阈值 `WW_LOG_COMPILE_THRESHOLD`（在 `ww_log_config.h` 中定义）：
高于阈值的 `LOG_DBG` / `LOG_INF` 调用在编译时直接消除。

### 5.2 动态开关（运行期）

两个全局变量，可随时修改：

| 变量 | 类型 | 默认值 | 含义 |
|------|------|--------|------|
| `g_ww_log_module_mask` | `U32` | `0xFFFFFFFF` | 位图，bit N = 模块 N 是否输出 |
| `g_ww_log_level_threshold` | `U8` | `WW_LOG_LEVEL_DBG` | 高于此 level 的日志被过滤 |

操作 API：

```c
// 模块开关
ww_log_set_module_mask(U32 mask);          // 直接设置掩码
ww_log_enable_module(U8 module_id);        // 开启指定模块
ww_log_disable_module(U8 module_id);       // 关闭指定模块
U8 ww_log_is_module_enabled(U8 module_id); // 查询

// level 过滤
ww_log_set_level_threshold(U8 level);      // 只输出 <= level 的日志
U8 ww_log_get_level_threshold(void);
```

level 枚举：

```c
#define WW_LOG_LEVEL_ERR  0   // 错误（最高优先级）
#define WW_LOG_LEVEL_WRN  1   // 警告
#define WW_LOG_LEVEL_INF  2   // 信息
#define WW_LOG_LEVEL_DBG  3   // 调试（最低优先级）
```

过滤逻辑集中在输出函数内部，调用点体积最小：

```c
if ((g_ww_log_module_mask & (1U << module_id)) == 0) return;
if (level > g_ww_log_level_threshold)               return;
```

> **开关粒度说明**：动态开关以**模块**为粒度，key 为 module_id（config 分配，稳定不变）。
> file_id 内的 offset 增删不影响开关行为。

---

## 6. 输出后端

在 `include/ww_log_config.h` 里选择后端组合，三者可**任意叠加**：

```c
#define WW_LOG_BACKEND_UART     1   // 串口实时输出
#define WW_LOG_BACKEND_RAM      1   // 4KB 环形缓冲（掉电不丢 RAM 区）
#define WW_LOG_BACKEND_STORAGE  0   // 外存（EEPROM / Flash），需平台 HAL
```

调用点对后端组合**完全无感**，输出路径如下：

```
LOG_INF(...)
  └→ ww_log_encode_output()   [过滤：模块mask + level + panic bypass]
       └→ ww_log_backend_emit(encoded, params, param_count, sync)
            ├→ [UART]    → printf hex 帧（encode）/ 可读文本（str）
            └→ [RAM]     → log_ram_write() → 写环形缓冲
                               └→ [达 flush 阈值] → log_flush_request()
                                      └→ [STORAGE] → 写 LOGH 块到外存
```

### 6.1 UART 后端

encode 模式：每条日志输出一行 hex 帧，格式 `0x<header> [0x<param1> ...]`：

```
0x08000441 0x00000000
0x08001040
0x08002081 0x0001C200 0x00000010
```

string 模式：直接输出可读文本：

```
[INF] demo_init.c:17 - Hardware check passed, code=0
[DBG] drv_uart.c:8 - UART FIFO depth=16
```

### 6.2 RAM 后端（4 KB 环形缓冲）

| 参数 | 值 | 说明 |
|------|----|------|
| 总大小 | 4096 B | 对应掉电不丢 RAM 维护区 |
| header | 64 B | magic、version、读写指针、校验和等 |
| 数据区 | 3968 B | 环形，可回绕 |
| flush 阈值 | 3008 B | 使用量超过此值自动触发 flush（若 STORAGE 开启） |

**热重启恢复**：初始化时若 header magic 和校验和有效，保留已有数据继续追加，不清零。

**dump 接口**（仅 `SIMULATION_MODE`，用于 PC 仿真取出数据）：

```c
// 原始 4KB 区域写入文件
ww_log_ram_dump_file("ram_dump.bin", WW_LOG_DUMP_BIN);
// 解析环形缓冲内容，输出 hex 帧文本
ww_log_ram_dump_file("ram_dump.hex", WW_LOG_DUMP_HEX);
```

真机通过 JTAG / 调试器直接读取 RAM 维护区地址段，存成 `.bin` 文件后用 decoder 解析。

### 6.3 STORAGE 后端（外存块环）

flush 时把 RAM 数据打包为 `LOGH` 块写入外存 LOG 分区：

| LOGH 块 header（32 B） | 含义 |
|------------------------|------|
| `magic = 0x4C4F4748` | 块起始标记 |
| `sequence` | 递增块序号（用于排序和定位写游标） |
| `timestamp` | 系统 tick |
| `data_size` | 块内数据字节数 |
| `entry_count` | 近似日志条数 |
| `ram_overflow` | flush 时 RAM 是否溢出过 |
| `checksum` | header 前 28 字节校验和 |

块依次追加到外存 LOG 分区，**写满后绕回覆盖最旧块**（块环，不是覆盖最新），历史日志最大限度保留。

---

## 7. panic 模式

在 HardFault、watchdog 超时等崩溃处理函数中调用：

```c
void ww_log_panic(void);
```

调用后：

1. 置 `g_ww_log_panic_flag = 1`——后续所有 LOG 调用**绕过模块 mask 和 level 过滤**。
2. 立即同步 flush RAM → 外存（不等阈值，若 STORAGE 开启）。
3. UART 切轮询模式（不依赖中断 / DMA）。
4. 后续日志同步直写（不走缓冲）。

目的：在系统彻底宕机前，把 RAM 维护区里最后几条日志安全落地到外存。

---

## 8. 目录结构与文件说明

```
ww_log_v1/
├── README.md                   本文档
├── Makefile                    构建脚本
├── log_config.json             人工维护：模块 → 目录映射
├── ww_log_map.json             脚本生成：构建 + decode 共用映射文件
│
├── include/                    公共头文件（按需 include）
│   ├── type.h
│   ├── ww_log.h
│   ├── ww_log_config.h
│   ├── ww_log_output.h
│   ├── ww_log_ctrl.h
│   ├── ww_log_panic.h
│   ├── ww_log_store.h
│   └── auto_file_ids.h         脚本生成
│
├── core/                       核心实现
│   ├── ww_log_ctrl.c
│   ├── ww_log_output.c
│   ├── ww_log_panic.c
│   └── ww_log_store.c
│
├── sim/                        PC 仿真外存
│   ├── sim_storage.h
│   └── sim_storage.c
│
├── scripts/log/
│   ├── log_config.json         同根目录 log_config.json 的软链接/副本
│   └── gen_log_map.py          map 生成器
│
├── tools/
│   └── log_decoder.py          日志解码器
│
├── src/                        Demo 模块（可替换为实际业务源码）
│   ├── demo/
│   │   ├── demo_in.h
│   │   ├── demo_init.c
│   │   └── demo_process.c
│   ├── drivers/
│   │   ├── drv_uart.h
│   │   └── drv_uart.c
│   └── test/
│       ├── test_unit.h
│       └── test_unit.c
│
├── examples/
│   └── main.c                  仿真入口：验证所有功能路径
│
└── output/                     构建产物（make 自动生成）
    ├── ww_log_sim.exe
    ├── auto_file_ids.h
    ├── file_ids.mk
    └── *.o  *.d
```

---

### 8.1 头文件详解

#### `include/type.h`

定义跨平台整数类型别名，整个项目统一使用：

```c
typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef int8_t   S8;
typedef int16_t  S16;
typedef int32_t  S32;
```

#### `include/ww_log.h`

项目的**唯一入口头文件**，使用方只需 `#include "ww_log.h"`。

功能：
- 定义编译模式（三选一宏）。
- 定义 level 枚举值（`WW_LOG_LEVEL_ERR/WRN/INF/DBG`）。
- 声明初始化和 panic 接口：

```c
void ww_log_init(void);   // 系统初始化（初始化后端、恢复 RAM 数据）
void ww_log_panic(void);  // 进入 panic 模式（崩溃处理函数调用）
```

- 根据模式分发包含 `ww_log_output.h`（LOG 宏）或展开为空。

#### `include/ww_log_config.h`

**所有编译期配置集中在此**，移植时主要修改这个文件：

```c
// 仿真模式（PC 编译时开启，目标板注释掉）
#define SIMULATION_MODE

// 后端开关（可任意组合，默认 UART + RAM）
#define WW_LOG_BACKEND_UART     1
#define WW_LOG_BACKEND_RAM      1
#define WW_LOG_BACKEND_STORAGE  0

// RAM 维护区地址（真机上指向掉电不丢的 SRAM 段）
#define DLM_MAINTAIN_LOG_BASE_ADDR   /* 平台相关，仿真为静态数组 */
#define DLM_MAINTAIN_LOG_SIZE        4096

// RAM 区布局
#define LOG_RAM_HEADER_SIZE          64
#define LOG_RAM_DATA_SIZE            3968
#define LOG_RAM_FLUSH_THRESHOLD      3008   // 超过此值触发 flush

// 外存分区
#define LOG_STORAGE_PARTITION_SIZE   4096
#define LOG_BLOCK_HEADER_SIZE        32

// 魔数与版本
#define LOG_RAM_MAGIC      0x574C4F47   // 'WLOG'
#define LOG_BLOCK_MAGIC    0x4C4F4748   // 'LOGH'
#define LOG_RAM_VERSION    0x00030000   // v1 (3.0.0)
```

#### `include/ww_log_output.h`

定义四个日志宏及编码相关宏，**不要直接 include，由 `ww_log.h` 按模式分发**。

用户使用的宏：

```c
LOG_ERR(fmt, ...)   // level=ERR (0)
LOG_WRN(fmt, ...)   // level=WRN (1)
LOG_INF(fmt, ...)   // level=INF (2)
LOG_DBG(fmt, ...)   // level=DBG (3)
```

encode 编码宏（供内部和 decoder 使用）：

```c
#define WW_LOG_ENCODE(file_id, line, pcnt) \
    ( (((U32)(file_id) & 0xFFF) << 20) | \
      (((U32)(line)    & 0x3FFF) << 6) | \
      ( (U32)(pcnt)    & 0x3F) )

#define WW_LOG_FILEID_OF(encoded)  (((encoded) >> 20) & 0xFFF)
#define WW_LOG_LINE_OF(encoded)    (((encoded) >> 6)  & 0x3FFF)
#define WW_LOG_PCNT_OF(encoded)    ((encoded) & 0x3F)
#define N_WW_LOG_MODULE_OF(file_id)  (((file_id) >> 7) & 0x1F)
#define WW_LOG_OFFSET_OF(file_id)  ((file_id) & 0x7F)
```

底层输出接口（供 core 调用）：

```c
void ww_log_backend_emit(U32 encoded, const U32 *params, U8 param_count, U8 sync);
```

#### `include/ww_log_ctrl.h`

运行期动态开关 API：

```c
extern U32 g_ww_log_module_mask;        // 模块使能位图
extern U8  g_ww_log_level_threshold;    // level 阈值

void ww_log_set_module_mask(U32 mask);
U32  ww_log_get_module_mask(void);
void ww_log_enable_module(U8 module_id);
void ww_log_disable_module(U8 module_id);
U8   ww_log_is_module_enabled(U8 module_id);

void ww_log_set_level_threshold(U8 level);
U8   ww_log_get_level_threshold(void);
```

#### `include/ww_log_panic.h`

panic 模式接口（`ww_log.h` 中已声明 `ww_log_panic()`，本头文件提供扩展接口）：

```c
extern U8 g_ww_log_panic_flag;   // 1 = 已进入 panic 模式
U8 ww_log_is_panic(void);        // 查询 panic 状态
```

仅 `SIMULATION_MODE` 下可用的 dump 辅助函数：

```c
typedef enum {
    WW_LOG_DUMP_BIN = 0,  // 原始二进制
    WW_LOG_DUMP_HEX = 1   // hex 帧文本
} WW_LOG_DUMP_FMT_E;

// 将 RAM 维护区内容写入文件（供 PC 仿真取出数据验证）
int ww_log_ram_dump_file(const char *path, WW_LOG_DUMP_FMT_E fmt);

// 将外存 LOG 分区内容写入文件（需 STORAGE 开启）
int ww_log_storage_dump_file(const char *path, WW_LOG_DUMP_FMT_E fmt);
```

#### `include/ww_log_store.h`

RAM 环形缓冲、外存存储、flush 引擎的完整类型定义和 API 声明。

**RAM header 结构体**（映射到 RAM 维护区前 64 字节）：

```c
typedef struct {
    U32 magic;            // LOG_RAM_MAGIC = 'WLOG'
    U32 version;          // LOG_RAM_VERSION
    U16 write_index;      // 写指针（相对数据区起始的字节偏移）
    U16 read_index;       // 读指针（已 flush 到此处）
    U32 total_written;    // 累计写入字节数（含回绕）
    U32 flush_count;      // 已 flush 次数
    U32 last_flush_time;  // 最后一次 flush 的时间戳
    U8  overflow_flag;    // 环形缓冲已回绕过（数据被覆盖）
    U8  reserved[35];
    U32 checksum;         // header 前 60 字节的校验和
} LOG_RAM_HEADER_T;
```

**RAM API**：

```c
void log_ram_init(U8 force_clear);           // 初始化（0=热重启恢复，1=强制清零）
int  log_ram_write(U32 encoded, U32 *params, U8 param_count);  // 写一条 entry
U8   log_ram_need_flush(void);               // 是否达到 flush 阈值
U16  log_ram_get_usage(void);                // 当前使用字节数
U16  log_ram_get_available(void);            // 剩余可用字节数
int  log_ram_read(U8 *buf, U16 max, U16 *actual);   // 读出待 flush 数据
void log_ram_clear_flushed(U16 size);        // 推进读指针（flush 成功后调用）
void log_ram_clear_all(void);                // 清空所有数据
```

**外存块 header 结构体**：

```c
typedef struct {
    U32 magic;         // LOG_BLOCK_MAGIC = 'LOGH'
    U32 sequence;      // 递增块序号
    U32 timestamp;     // 系统 tick
    U16 data_size;     // 本块数据字节数
    U16 entry_count;   // 近似日志条数
    U8  ram_overflow;  // flush 时 RAM 是否溢出
    U8  reserved[11];
    U32 checksum;      // header 前 28 字节校验和
} LOG_BLOCK_HEADER_T;
```

**flush 引擎 API**：

```c
int log_flush_request(U8 force);  // 请求 flush（force=1 忽略阈值判断）
int log_flush_process(void);      // 执行一次 flush（非阻塞，适合周期调用）
int log_flush_now(void);          // 同步立即 flush（panic 模式使用）
```

#### `include/auto_file_ids.h`（生成，不手动编辑）

由 `gen_log_map.py --header` 生成，包含每个模块的 ID 宏：

```c
#define WW_LOG_MODULE_DEMO    1
#define WW_LOG_MODULE_TEST    2
#define WW_LOG_MODULE_DRIVERS 4
#define WW_LOG_MODULE_MAX     5
```

---

### 8.2 核心实现文件详解

#### `core/ww_log_ctrl.c`

定义全局变量并实现运行期控制函数：

- `g_ww_log_module_mask`：初始值 `0xFFFFFFFF`（所有模块开启）。
- `g_ww_log_level_threshold`：初始值 `WW_LOG_LEVEL_DBG`（全部输出）。
- `ww_log_init()`：打印模式和后端信息，调用 `log_ram_init(0)` 尝试恢复热重启数据，若 STORAGE 开启则依次初始化外存相关子系统。

#### `core/ww_log_output.c`

encode 模式和 string 模式的输出函数实现，以及后端分发：

- `ww_log_encode_output(file_id, line, level, param_count, ...)`
  1. panic 检查 → 决定是否跳过过滤。
  2. 检查 module mask 和 level 阈值。
  3. 收集可变参数到 U32 数组（最多 16 个）。
  4. 编码头：`WW_LOG_ENCODE(file_id, line, param_count)`。
  5. 调用 `ww_log_backend_emit()`。

- `ww_log_str_output(module_id, filename, line, level, fmt, ...)`
  1. 同样执行模块/level 过滤。
  2. `printf("[LEVEL] filename:line - <msg>\n")`。

- `ww_log_backend_emit(encoded, params, param_count, sync)`
  - 依次分发给所有开启的后端（UART、RAM）。
  - RAM 写入后检查 flush 需求，若需要调用 flush。

#### `core/ww_log_panic.c`

- 实现 `ww_log_panic()`（见 §7）。
- SIMULATION_MODE 下实现 `ww_log_ram_dump_file()` 和 `ww_log_storage_dump_file()`：
  - BIN 格式：整块区域原始写入文件。
  - HEX 格式：解析环形缓冲或块链，每条 entry 输出为 `0x... [0x...]` 文本行。

#### `core/ww_log_store.c`（约 800 行，存储引擎核心）

实现三个子系统：

**RAM 环形缓冲（`log_ram_*`）**

- `log_ram_init(force_clear)`：映射 header 和 data 指针到 RAM 维护区，校验 magic 和 checksum 决定是否保留数据。
- `log_ram_write()`：追加 entry，处理回绕，更新 `total_written` 和 header checksum。若已用量 ≥ `LOG_RAM_FLUSH_THRESHOLD` 返回 1（触发 flush 信号）。
- `log_ram_read()`：顺序读出 `[read_index, write_index)` 区间，正确处理回绕。
- `log_ram_clear_flushed(size)`：推进 read_index，释放已读区域。

**外存存储（`log_storage_*`）**

- `log_storage_init()`：定位 LOG 分区，初始化写游标。
- `log_storage_write/read/erase()`：向 EEPROM/Flash HAL 分发操作，内置重试逻辑（最多 3 次）。

**块链与 flush 引擎（`log_header_*` / `log_flush_*`）**

- `log_header_build()`：填充 32 字节 LOGH header，递增 sequence，计算 checksum。
- `log_header_scan_max_sequence()`：启动时扫描外存分区，找到当前最大 sequence，确定写游标位置。
- `log_flush_now()`：同步读 RAM → 构建 LOGH 块 → 写外存 → 推进读指针。

---

### 8.3 工具脚本详解

#### `scripts/log/gen_log_map.py`

**三种调用方式**：

```bash
# 生成 / 更新 ww_log_map.json（每次构建前自动调用）
python3 scripts/log/gen_log_map.py log_config.json

# 同上，同时输出 file_ids.mk 到 stdout（供 Makefile 重定向）
python3 scripts/log/gen_log_map.py log_config.json --makefile

# 同上，同时输出 auto_file_ids.h 到 stdout
python3 scripts/log/gen_log_map.py log_config.json --header
```

**关键逻辑**：

| 步骤 | 说明 |
|------|------|
| 加载 config | 解析模块 ID、目录、enable |
| 读旧 map | 提取已分配的 `(path → offset)` 映射，用于锁定 |
| 扫描 .c | 在所有 `dirs` 下递归查找 `.c` 文件，按最长路径前缀分配模块 |
| 分配 offset | 已知文件保持旧 offset；新文件取最低空闲 offset；删除文件预留占位 |
| 提取 entry | 正则提取 `LOG_ERR/WRN/INF/DBG(...)` 调用，解析 fmt 和参数个数，校验占位符数量 |
| 输出 | `ww_log_map.json` / `file_ids.mk` / `auto_file_ids.h` |

**`file_ids.mk` 格式示例**（供 Makefile `-D` 注入）：

```makefile
FILE_ID_src_demo_demo_init_c         = 128
MODULE_ID_src_demo_demo_init_c       = 1
MODULE_STATIC_EN_src_demo_demo_init_c = 1

FILE_ID_src_drivers_drv_uart_c       = 512
MODULE_ID_src_drivers_drv_uart_c     = 4
MODULE_STATIC_EN_src_drivers_drv_uart_c = 1
```

#### `tools/log_decoder.py`

把二进制或 hex 文本日志还原为可读格式。

**调用方式**：

```bash
# 从 stdin 解码（UART 实时管道）
./output/ww_log_sim | python3 tools/log_decoder.py --map ww_log_map.json -

# 解码文件（自动识别格式：hex 文本 / WLOG RAM 区 / LOGH 块链 / 裸流）
python3 tools/log_decoder.py --map ww_log_map.json dump.bin

# 直接解码一段 hex 字符串
python3 tools/log_decoder.py --map ww_log_map.json --hex "0x08000441 0x00000000"

# 强制指定格式
python3 tools/log_decoder.py --map ww_log_map.json --format bin part.bin
python3 tools/log_decoder.py --map ww_log_map.json --format hex capture.txt
```

**自动格式识别规则**：

| 特征 | 识别为 |
|------|--------|
| 可打印字符为主 + 含 `0x` 字样 | hex 文本流 |
| 偏移 0 处 magic = `'WLOG'` | RAM 维护区（含 header 和环形缓冲） |
| 含 `'LOGH'` magic | 外存 LOGH 块链 |
| 其他 | 裸二进制 entry 流（小端 U32） |

**输出格式**：

```
[INF] demo_init.c:17 - Hardware check passed, code=0
[WRN] demo_process.c:22 - Result is large, id=42, result=84
[DBG] drv_uart.c:9 - UART FIFO depth=16
```

%s 参数显示占位：`<%s@0xXXXXXXXX>`。

---

### 8.4 仿真外存（`sim/`）

仅在 PC 仿真（`SIMULATION_MODE`）下使用，把 EEPROM/Flash 操作映射到本地文件：

- `sim_storage.h`：声明初始化、读写擦 API，供 `ww_log_store.c` 内的 HAL 调用点使用。
- `sim_storage.c`：用 `fopen/fread/fwrite` 模拟块设备，文件存放在 `sim_data/` 目录下。

移植到真实硬件时，用实际的 EEPROM/Flash 驱动替换 `sim_storage.c` 中的实现即可，接口不变。

---

### 8.5 Demo 模块（`src/`）

演示如何在业务代码中使用日志系统，可直接参考其结构移植。

#### `src/demo/demo_in.h`

```c
#include "ww_log.h"
#include "auto_file_ids.h"   // 每个 .c 文件需要 include 这两个头文件
```

#### `src/demo/demo_init.c`

```c
void demo_init(void) {
    LOG_INF("Demo module initializing...");
    LOG_DBG("Checking hardware...");
    if (status == 0) {
        LOG_INF("Hardware check passed, code=%d", status);
    } else {
        LOG_ERR("Hardware check failed!");
    }
    LOG_WRN("Demo init completed with warnings, total=%d, failed=%d", 5, 1);
}
```

#### `src/demo/demo_process.c`

```c
void demo_process(int task_id) {
    LOG_DBG("Processing task...");
    if (task_id < 0) { LOG_ERR("Invalid task ID!"); return; }
    LOG_INF("Task started, id=%d", task_id);
    int result = task_id * 2;
    if (result > 100) LOG_WRN("Result is large, id=%d, result=%d", task_id, result);
    LOG_INF("Task completed, id=%d, result=%d", task_id, result);
}
```

#### `src/test/test_unit.c`

TEST 模块在 `log_config.json` 中 `"enable": false`，所有 LOG 调用在编译期展开为空，
不产生任何代码，可用于验证静态关闭效果。

---

### 8.6 `examples/main.c`

仿真入口，覆盖所有功能路径（可作为集成测试）：

```
1. ww_log_init()
2. 正常日志路径：demo_init / demo_process / drv_uart_init / drv_uart_send / test_unit_run
3. 动态模块开关：disable DEMO → 调用 demo_init（无输出）→ enable DEMO
4. 动态 level 阈值：阈值设为 ERR → 调用 demo_process（只有 ERR 输出）→ 还原
5. panic 模式：ww_log_panic() → demo_process（绕过全部过滤）
6. dump 文件（SIMULATION_MODE）：ram_dump.bin / ram_dump.hex / storage_dump.hex
```

---

## 9. 快速开始

### 9.1 环境要求

- **PC 仿真**：GCC（MinGW-w64 / Linux GCC）、GNU Make、Python 3.7+
- **真机**：任意 C99 工具链；Python 只在 PC 端解码时需要

### 9.2 构建与运行

```bash
cd ww_log_v1

make            # 生成 map → 编译 → 输出 output/ww_log_sim.exe
make run        # 编译并运行仿真
make map        # 仅重新生成 ww_log_map.json 及派生文件
make clean      # 清理构建产物
make distclean  # 同上，并清除 ww_log_map.json 和 sim_data/
```

### 9.3 切换编译模式

编辑 `include/ww_log.h`，三选一后 `make`：

```c
// 生产（encode）：注释其他，保留：
#define WW_LOG_MODE_ENCODE

// 调试（string）：注释其他，保留：
#define WW_LOG_MODE_STR

// 全关：注释其他，保留：
#define WW_LOG_MODE_DISABLED
```

### 9.4 添加新模块

1. 在 `log_config.json` 添加模块条目，分配一个**未使用的 `id`（0–31）**。
2. 在对应目录下新建 `.c` 文件，每个文件顶部：
   ```c
   #include "ww_log.h"
   #include "auto_file_ids.h"
   ```
3. `make`（或 `make map`）自动扫描、分配 file_id、注入编译宏。

### 9.5 运行期动态控制示例

```c
ww_log_init();

// 关闭某模块的日志（module_id 见 auto_file_ids.h）
ww_log_disable_module(WW_LOG_MODULE_DEMO);

// 只输出 ERR 级别
ww_log_set_level_threshold(WW_LOG_LEVEL_ERR);

// 恢复
ww_log_enable_module(WW_LOG_MODULE_DEMO);
ww_log_set_level_threshold(WW_LOG_LEVEL_DBG);
```

---

## 10. encode 解码闭环

### 10.1 使用 `log_decoder.py`

```bash
# UART 实时管道（仿真）
./output/ww_log_sim | python3 tools/log_decoder.py --map ww_log_map.json -

# 真机 UART 抓取的文本文件
python3 tools/log_decoder.py --map ww_log_map.json capture.txt

# 真机 JTAG/读回的 RAM 维护区二进制（4KB）
python3 tools/log_decoder.py --map ww_log_map.json ram_region.bin

# 真机外存 LOG 分区二进制
python3 tools/log_decoder.py --map ww_log_map.json log_partition.bin

# 直接解析单条 hex（调试用）
python3 tools/log_decoder.py --map ww_log_map.json --hex "0x08000441 0x00000000"
```

### 10.2 版本管理要点

| 规则 | 原因 |
|------|------|
| 每次发布固件时，与固件**一起归档** `ww_log_map.json` | decode 必须用构建时的 map |
| 不要手动修改 `ww_log_map.json` | 由脚本维护，手动改可能破坏 file_id 锁定 |
| 源码增删文件后执行 `make map` | 更新 map，旧 offset 保持不变 |
| `module_id` 一旦分配**绝不更改** | 它是运行期动态开关的稳定 key |

### 10.3 验证 file_id 锁定

```bash
# 第一次生成
make map
# 记录 ww_log_map.json 里 file_id → path 的对应关系

# 新增一个 .c 文件，重新生成
make map
# 验证：旧文件的 file_id 完全不变，新文件获得新 offset
```

---

## 11. 真机集成指南

### 11.1 移植步骤

1. **关闭仿真模式**：注释 `ww_log_config.h` 中的 `#define SIMULATION_MODE`。
2. **配置 RAM 维护区地址**：把 `DLM_MAINTAIN_LOG_BASE_ADDR` 指向掉电不丢的 SRAM 段起始地址（如链接脚本中的 `.maintain` 节）。
3. **配置后端**：按需开启 `WW_LOG_BACKEND_RAM` / `WW_LOG_BACKEND_STORAGE`。
4. **实现 HAL**（仅 STORAGE 需要）：替换 `sim/sim_storage.c`，实现以下接口：
   ```c
   void sim_storage_init(void);
   int  sim_storage_read(U32 offset, U8 *buf, U32 size);
   int  sim_storage_write(U32 offset, const U8 *buf, U32 size);
   int  sim_storage_erase(U32 offset, U32 size);
   ```
5. **Makefile 适配**：把 `per-file` 编译规则（注入 `CURRENT_FILE_ID` 等三个宏）迁移到目标构建系统。
6. **在启动代码中调用** `ww_log_init()`；**在 HardFault handler 中调用** `ww_log_panic()`。

### 11.2 取日志方式

| 场景 | 操作 |
|------|------|
| UART 实时监控 | 串口工具记录 hex 文本，事后 `log_decoder.py` 解析 |
| 崩溃后读 RAM | JTAG 把 `DLM_MAINTAIN_LOG_BASE_ADDR` 处 4 KB dump 成文件 |
| 读外存 LOG 分区 | JTAG / 厂测工具 dump LOG 分区成文件 |

所有格式（hex 文本 / WLOG binary / LOGH binary）均可直接喂给 `log_decoder.py`，自动识别。

### 11.3 临界区注意事项

RAM 环形缓冲的 `write_index` / `read_index` 更新是非原子操作，如果日志在中断和任务中同时调用，需要在 `log_ram_write()` 调用前后加临界区保护（关中断或互斥量）。当前实现没有内置锁，由使用方根据 RTOS 选择合适的保护方式。

---

## 12. 与 v0 的主要差异

| 维度 | v0 | v1 |
|------|----|----|
| 编码头布局 | `[file12][line12][datalen6][level2]` | `[file12][line14][pcnt6]`，**level 不进编码** |
| line 上限 | 4095 | **16383** |
| file_id 内部划分 | module × 64 + offset（4+8 位） | module × 128 + offset（**5+7 位**） |
| 文件 ID 分配 | 人工在 config 里逐一填写 `files` 表 | **脚本自动扫描分配，offset 锁定** |
| 映射文件 | 构建和解码各用一套 | **统一 `ww_log_map.json`，一文件两用** |
| 后端组合 | RAM / UART 二选一编译 | **UART / RAM / STORAGE 任意组合叠加** |
| panic 模式 | 无 | **有（崩溃时强制 flush 保日志）** |
| 外存写入策略 | 单块覆盖 | **块环追加（历史日志最大保留）** |
| 参数格式校验 | 无 | **构建期统计占位符与传参数量，不符发告警** |

---

## 附录：文件速查表

| 文件 | 类型 | 主要内容 |
|------|------|---------|
| `include/type.h` | 头文件 | U8/U16/U32/S8/S16/S32 类型别名 |
| `include/ww_log.h` | 头文件 | **用户唯一入口**：模式宏、level 定义、init/panic 声明 |
| `include/ww_log_config.h` | 头文件 | **移植配置**：仿真开关、后端组合、RAM/外存尺寸、magic |
| `include/ww_log_output.h` | 头文件 | LOG_ERR/WRN/INF/DBG 宏定义、编码宏、backend_emit 声明 |
| `include/ww_log_ctrl.h` | 头文件 | 模块 mask + level 阈值的 API 声明和全局变量 extern |
| `include/ww_log_panic.h` | 头文件 | panic flag、is_panic()、仿真 dump 接口 |
| `include/ww_log_store.h` | 头文件 | RAM/外存类型定义（header 结构体）、全量 API 声明 |
| `include/auto_file_ids.h` | 头文件（生成） | WW_LOG_MODULE_XXX 宏 |
| `core/ww_log_ctrl.c` | 源文件 | 全局变量定义、init、模块/level 控制函数 |
| `core/ww_log_output.c` | 源文件 | encode/string 输出函数、后端分发实现 |
| `core/ww_log_panic.c` | 源文件 | panic 函数、RAM/外存 dump 实现 |
| `core/ww_log_store.c` | 源文件 | RAM 环形缓冲、外存存储、块链与 flush 引擎（约 800 行） |
| `sim/sim_storage.h` | 头文件 | 仿真外存 HAL 接口声明 |
| `sim/sim_storage.c` | 源文件 | 基于本地文件的 EEPROM/Flash 仿真 |
| `scripts/log/log_config.json` | 配置 | 模块 ID、目录、静态开关（**人工维护**） |
| `scripts/log/gen_log_map.py` | Python | 扫描 .c → ww_log_map.json / file_ids.mk / auto_file_ids.h |
| `tools/log_decoder.py` | Python | hex 文本 / 二进制 → 可读日志（自动识别格式） |
| `ww_log_map.json` | JSON（生成） | 条目映射（file_id, line, level, fmt）、文件表、模块表 |
| `Makefile` | Make | 构建目标：all / run / map / clean / distclean |
| `examples/main.c` | 源文件 | 仿真驱动：覆盖全部功能路径（可作集成测试） |
| `src/demo/*.c` | 源文件 | DEMO 模块示例（参考移植用） |
| `src/drivers/*.c` | 源文件 | DRIVERS 模块示例 |
| `src/test/*.c` | 源文件 | TEST 模块示例（静态关闭，验证 enable=false） |
