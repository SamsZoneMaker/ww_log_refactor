# ww_log v1 — 嵌入式日志系统（新方案）

> 面向固件体积受限场景的日志系统重构。核心目标：用 **encode 模式**把每条
> 日志压成定长二进制，大幅省 ROM/RAM；同时保留 **string 模式**（调试用）和
> **全关模式**，三者编译期切换、调用点写法完全一致。

---

## 1. 为什么要重构

原 string 模式把格式串、文件名等全部编进固件，体积太大。新方案的思路：

- 调用点只产生一个**定长二进制 entry**（4 字节头 + 若干 U32 参数），格式串
  完全不进固件。
- entry 可直接写进掉电不丢的 **RAM 维护区（4KB）**，再 flush 到外存。
- PC 端用一份**映射文件**把二进制还原成可读日志。

一句话：**设备只存 ID + 参数，文本留在 PC。**

---

## 2. 三种模式（编译期三选一）

在 `include/ww_log.h` 里三选一：

```c
#define WW_LOG_MODE_STR        // string：printf 风格，直观，体积大（调试）
// #define WW_LOG_MODE_ENCODE  // encode：定长二进制，省体积（生产）
// #define WW_LOG_MODE_DISABLED // 全关：所有 LOG 宏展开为空
```

无论哪种模式，调用点写法都一样：

```c
LOG_ERR("msg");                 LOG_WRN("x=%d", a);
LOG_INF("x=%d y=%d", a, b);     LOG_DBG("...");
```

三种模式实际输出对比（同一份源码）：

| 模式 | 输出 |
|------|------|
| STR | `[INF] demo_init.c:17 - Hardware check passed, code=0` |
| ENCODE | `0x08000441 0x00000000`（U32 头 + 参数，hex 帧） |
| DISABLED | （无任何输出，宏展开为空，零体积） |

---

## 3. encode 编码格式（核心）

每条 log 头部是一个 **U32**：

```
 31                20 19              6 5         0
┌────────────────────┬──────────────────┬───────────┐
│   file_id (12)     │    line (14)     │ param_cnt │
│                    │                  │   (6)     │
└────────────────────┴──────────────────┴───────────┘
        │
        └ file_id = [ module_id : 5 ][ offset : 7 ]
```

| 字段 | 位宽 | 范围 | 含义 |
|------|------|------|------|
| file_id | 12 | 0–4095 | 高 5 位 = module_id(0–31)，低 7 位 = 模块内 offset(0–127) |
| line | 14 | 0–16383 | `__LINE__` |
| param_count | 6 | 0–63 | 后跟的 U32 参数个数（让缓冲区**自描述**） |

U32 头之后紧跟 `param_count` 个 U32 参数。一条完整 entry =
`4 + param_count*4` 字节。

```c
#define WW_LOG_ENCODE(file_id, line, pcnt) \
    ( (((U32)(file_id) & 0xFFF) << 20) | \
      (((U32)(line)    & 0x3FFF) << 6) | \
      ( (U32)(pcnt)    & 0x3F) )
```

### level 不进编码

level 只在编码**前**用于过滤（`level > 阈值` 直接 return），decode 时再由 map
按 `(file_id, line)` 还原。这样头部省下 2 bit 给了 line（4095 → 16383）。

### `%s` 的处理（告警不报错）

`%s` 对应的指针照常当 U32 存入缓冲（浪费 4 字节但无害）；decode 时显示占位
`<%s@0xXXXXXXXX>`（内容不可还原）；扫描器遇到 `%s` 只告警。

> 手算示例：`0x08000441 0x00000000`
> → file_id=`0x080`=128（demo_init.c），line=17，pcnt=1，参数=0
> → 查 map：`"Hardware check passed, code=%d"` → `code=0`

---

## 4. 无感的 ID 管理 + 统一 JSON

### 输入：`log_config.json`（人工维护，只登记 模块→目录）

```json
{
  "modules": {
    "DEMO":    { "id": 1, "dirs": ["src/demo"],               "enable": true },
    "TEST":    { "id": 2, "dirs": ["src/test"],               "enable": false },
    "DRIVERS": { "id": 4, "dirs": ["src/drivers", "src/hal"], "enable": true }
  },
  "unregistered": "warn"
}
```

- module 由**路径前缀匹配**判定，**最长前缀优先**。
- 一个模块可含多个目录、任意多文件。
- 不在任何 `dirs` 下的 `.c` → 告警 + 该文件 log 关闭（不阻断编译）。
- `module_id` 由人工分配且**稳定**（开关 key 在它上面），范围 0–31。

### 生成：`tools/gen_log_map.py` 扫描 → `ww_log_map.json`

脚本扫描 `dirs` 下所有 `.c`，提取每个 `LOG_xxx(...)` 的 **行号 + level + fmt +
参数个数**，产出统一映射文件：

```json
{
  "meta":    { "version": "", "build_time": "...", "encoding": "file12_line14_pcnt6" },
  "modules": { "1": {"name": "DEMO", "enable": true}, ... },
  "files":   { "128": {"path": "src/demo/demo_init.c", "module": "DEMO", "present": true}, ... },
  "entries": [ {"file_id": 128, "line": 17, "level": "INF", "fmt": "Hardware check passed, code=%d"}, ... ]
}
```

### file_id 锁定（关键）

`file_id = module_id*128 + offset`。**重新生成时先读旧 `ww_log_map.json`**：

- 已分配过的文件 → 保持原 offset 不变；
- 新文件 → 取最低空闲 offset；
- 删除的文件 → offset **保留占位、不回收**（旧固件历史日志仍能正确 decode）。

### 一文件两用

`ww_log_map.json` 同时驱动构建和 decode：

```
gen_log_map.py log_config.json              # 扫描 → ww_log_map.json
gen_log_map.py log_config.json --makefile   # 派生 build/file_ids.mk（供 Makefile -D 注入）
gen_log_map.py log_config.json --header      # 派生 include/auto_file_ids.h（ID 宏）
log_decoder.py --map ww_log_map.json         # 直接读它做还原
```

### 构建期校验

扫描时统计 fmt 里非 `%%` 的占位符个数，与实际传参个数比对，不一致**告警**。

---

## 5. 开关设计（string / encode 都支持）

两层，互不依赖：

### 静态开关（编译期，零代码）

Makefile 按文件注入 `CURRENT_MODULE_STATIC_EN`（来自该文件所属模块的
`enable`）。为 0 时 LOG 宏展开为空，**该文件 log 零体积**（用 `_WW_LOG_IF(cond)`
token 拼接技巧实现）。

另有编译期 level 阈值 `WW_LOG_COMPILE_THRESHOLD`：高于阈值的 `LOG_DBG/INF`
直接编译掉。

### 动态开关（运行期）

```c
ww_log_set_module_mask(mask);        ww_log_enable_module(id);  ww_log_disable_module(id);
ww_log_set_level_threshold(level);
```

检查在输出函数内部集中完成（减小调用点体积）：

```c
if ((g_ww_log_module_mask & (1U << module_id)) == 0) return;
if (level > g_ww_log_level_threshold) return;
```

> 开关 key 在 **module_id**（config 分配，稳定），不在 offset 上。因此 file_id
> 锁定/漂移**不影响开关**。动态开关停在模块粒度，不依赖会变的 offset。

### 耦合点收敛

日志核心只认三个 Makefile 注入宏：`CURRENT_FILE_ID` / `CURRENT_MODULE_ID` /
`CURRENT_MODULE_STATIC_EN`。将来换构建系统，只需换"注入这三个宏"的方式，核心不动。

---

## 6. 输出后端（可组合）

`include/ww_log_config.h` 里三个独立开关，可任意叠加：

```c
#define WW_LOG_BACKEND_UART     1   // encode: hex 帧 / str: 可读文本
#define WW_LOG_BACKEND_RAM      1   // 默认开：4KB 环形缓冲（64B header + 数据区，3KB flush 阈值）
#define WW_LOG_BACKEND_STORAGE  0   // flush 到外存（EEPROM/Flash），按需 -D 开
```

**默认 `UART + RAM`**：encode 日志既从串口打 hex 帧，又 copy 一份进掉电不丢的 RAM
维护区。输出函数编码出 U32 后交给 `ww_log_backend_emit()` 分发，**调用点对后端组合
完全无感。**

### 取日志（dump）

- **真机**：用 JTAG 直接读 RAM 维护区地址段、或读外存 LOG 分区，存成文件即可。
- **仿真**：用便捷接口落地成文件（`include/ww_log_dump.h`，仅 `SIMULATION_MODE`）：

```c
ww_log_ram_dump_file("ram_dump.bin", WW_LOG_DUMP_BIN);   // 原始区快照
ww_log_ram_dump_file("ram_dump.hex", WW_LOG_DUMP_HEX);   // 0x.. hex 帧文本
ww_log_storage_dump_file("storage_dump.bin", WW_LOG_DUMP_BIN);  // 需 STORAGE
```

两种格式都能直接喂给 `log_decoder.py`（自动识别）。

### 外存刷入策略：块环，不覆盖

flush 把 RAM 数据打包成 `LOGH` 块，**依次往后追加**到外存分区；写满分区才绕回覆盖
最旧的块。所以历史会保留（不再是“每次刷同一位置只剩最后一块”）。重启时扫描块链
定位写游标续写（跨绕回的完整恢复留待后续硬化）。

---

## 7. panic 模式

```c
void ww_log_panic(void);
```

在 HardFault / watchdog 回调里调用，语义：

1. 绕过所有模块/level 过滤；
2. 立即同步 flush RAM → 外存（不等阈值）；
3. UART 切轮询输出（不依赖中断）；
4. 置 panic flag，后续 LOG 直接同步直写。

目的：靠 RAM/外存保住崩溃前最后几条日志。

---

## 8. 目录结构

```
ww_log_v1/
├── README.md                  ← 本文件
├── Makefile                   ← PC 仿真构建（mingw32-make + sh）
├── log_config.json            ← 人工：模块→目录
├── ww_log_map.json            ← 生成（构建 + decode 共用）
├── include/
│   ├── type.h                 ← U8/U16/U32
│   ├── ww_log.h               ← 模式分发 + 公共 API + level 宏
│   ├── ww_log_config.h        ← 后端开关 / RAM 尺寸 / 阈值 / magic
│   ├── ww_log_encode.h        ← encode 宏 + 输出声明
│   ├── ww_log_str.h           ← str 宏 + 输出声明
│   ├── ww_log_modules.h       ← 动态 mask + level 阈值 API
│   ├── ww_log_backend.h       ← 后端分发
│   └── auto_file_ids.h        ← 生成（模块/文件 ID 宏）
├── core/
│   ├── ww_log_common.c        ← init
│   ├── ww_log_modules.c       ← 全局开关变量 + API
│   ├── ww_log_encode.c        ← encode 输出
│   ├── ww_log_str.c           ← str 输出
│   └── ww_log_backend.c       ← 后端分发实现
├── sim/                       ← PC 仿真外存（沿用 v0 sim_storage.*）
├── tools/
│   ├── gen_log_map.py         ← 扫描 → ww_log_map.json / file_ids.mk / auto_file_ids.h
│   └── log_decoder.py         ← --map ww_log_map.json 还原
├── src/                       ← demo 模块（demo / drivers / test）
└── examples/main.c            ← 仿真主程序
```

---

## 9. 构建与使用

> 本机 `make` 即 MSYS2 的 `mingw32-make`（gcc 14 / GNU Make 4.4）。

```bash
cd ww_log_v1
make            # 生成 map → 派生 file_ids.mk/auto_file_ids.h → 编译
make run        # 编译并运行仿真
make map        # 仅重新生成 ww_log_map.json 及派生文件
make clean      # 清理 build/ bin/ 与生成的头文件
```

切模式：编辑 `include/ww_log.h` 的三选一宏，重新 `make`。

**encode 解码闭环**（生产模式回放）：

`log_decoder.py` 自动识别输入格式（`--format auto|hex|bin` 可强制），三种来源同一条命令：

```bash
# 1) hex 文本：UART 串口抓取 / .txt / .hex（PC 仿真用管道）
./bin/ww_log_sim | python3 tools/log_decoder.py --map ww_log_map.json -

# 2) 二进制 dump：真机 JTAG/读回的外存 LOG 分区或 4KB RAM 维护区
#    自动扫描 'LOGH'(外存块) / 'WLOG'(RAM 区) 魔数；整片芯片 dump（前导 0xFF）也行
python3 tools/log_decoder.py --map ww_log_map.json dump.bin

# 3) 直接解一段 hex（不用建文件）
python3 tools/log_decoder.py --map ww_log_map.json --hex "0x08000441 0x00000000"
```

> **关键**：解码用的 `ww_log_map.json` 必须与设备里固件的那次构建一致 —— 每次发版
> 把 map 和固件一起按版本归档（file_id 锁定保证旧固件历史日志仍可解）。

### 真机使用（非 sim）

| 通路 | 数据来源 | 解码 |
|------|---------|------|
| UART 在线 | 串口工具（PuTTY/teraterm/pyserial）落地为文本 | `log_decoder.py --map <版本map> capture.txt` |
| 外存离线 | JTAG/读回命令 dump LOG 分区为 .bin | `log_decoder.py --map <版本map> part.bin` |
| RAM 崩溃后 | 调试器 dump 4KB 维护区为 .bin | 同上（自动识别 'WLOG'） |

`%s` 仅存了指针，值无法还原，显示 `<%s@0x...>` 占位。

---

## 10. 与 v0 的主要差异

| 维度 | v0 | v1 |
|------|----|----|
| 编码头 | `[file12][line12][datalen6][level2]` | `[file12][line14][pcnt6]`，**level 不进编码** |
| line 上限 | 4095 | 16383 |
| file_id 划分 | module*64 + offset | module*128 + offset（5+7 位） |
| ID 来源 | 人工在 config 里写死 `files` 表 | **脚本扫描 .c 自动提取**，offset 锁定 |
| 映射文件 | 构建/解码各一套 | **统一 `ww_log_map.json`，一文件两用** |
| 后端 | RAM/UART 二选一编译 | **UART/RAM/STORAGE 可自由组合叠加** |
| panic | 无 | **有**（崩溃时强制 flush 保日志） |

---

## 11. 平台约定

- 类型用 `type.h` 的 `U8/U16/U32`。
- 命名：函数 `ww_log_<action>_<object>`，类型 `XXX_T/XXX_E`，配置宏 `WW_LOG_<FEATURE>`。
- 宏务必 `do{}while(0)` 包裹；注释 Doxygen 风格。
- 目标平台：RISC-V（Andes N25），裸机 / 轻 RTOS，无动态内存，RAM 紧张，UART 为主
  调试口。环形缓冲指针操作注意临界区原子性。

---

## 12. 实现进度

| 模块 | 状态 |
|------|------|
| type / config / 模式分发 / 开关层 | ✅ 已完成 |
| encode / str / backend(UART) | ✅ 已完成 |
| `gen_log_map.py`（扫描 + file_id 锁定 + 派生） | ✅ 已完成 |
| sim 验收：STR / ENCODE / DISABLED 切换 | ✅ 已通过 |
| sim 验收：静态/动态开关 | ✅ 已通过 |
| RAM 环形缓冲 / storage / flush | ✅ 已完成 |
| sim 外存（sim/sim_storage.*） | ✅ 已完成 |
| panic 模式 | ✅ 已完成 |
| `log_decoder.py` 解码闭环 | ✅ 已完成 |

### 后端组合构建（RAM/STORAGE 默认关，可叠加）

```bash
# 仅 UART（默认，include/ww_log_config.h）
make run

# 叠加 RAM + STORAGE（外存写入 + 达阈值/强制 flush）
make STATIC_OPTS='-DWW_LOG_BACKEND_RAM=1 -DWW_LOG_BACKEND_STORAGE=1' run
```

> 注：PC 仿真的“掉电不丢 RAM 区”是进程内静态数组，进程退出即清零，
> 因此热重启恢复（`log_ram_init(0)` 校验头保留数据）的逻辑已实现，但跨进程
> 复现需在真实硬件上验证。`%s` 参数值无法还原（仅存指针），decode 显示占位。
```
