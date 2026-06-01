# CLAUDE.md — ww_log v1 实现规格书

> 本文件是 **ww_log_v1** 的实现规格。`/clear` 之后的 Claude 直接照此编写代码。
> v0（旧实现）已整体移入 `ww_log_v0/`，**只读参考，不要修改**。
> 新代码全部写在 `ww_log_v1/`。

---

## 0. 项目背景（一句话）

嵌入式日志系统重构。痛点：原 string 模式在固件体积受限时太占空间。需要一个 **encode 模式**（把每条 log 压成定长二进制）大幅省 ROM/RAM，同时保留 string 模式（调试用）和全关模式。encode 出的数据可存进 RAM（4KB，掉电不丢的维护区）并 flush 到外存，PC 端用映射文件 decode 回可读日志。

---

## 1. 三种模式（编译期切换）

在 `ww_log.h` 里三选一（沿用 v0 风格）：

```c
// #define WW_LOG_MODE_STR        // string 模式：printf 风格，直观，体积大（调试用）
// #define WW_LOG_MODE_ENCODE     // encode 模式：定长二进制，省体积（生产用）
#define WW_LOG_MODE_DISABLED      // 全关：所有 LOG 宏展开为空
```

统一 API，三种模式调用点写法完全一致：

```c
LOG_ERR("msg");                 LOG_WRN("x=%d", a);
LOG_INF("x=%d y=%d", a, b);     LOG_DBG("...");
```

- `WW_LOG_MODE_DISABLED`：四个宏 `do{}while(0)`。
- 三种模式都必须支持 **level 开关** 和 **模块开关**（见 §4）。

---

## 2. encode 编码格式（核心，已锁定）

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
| param_count | 6 | 0–63 | 后跟的 U32 参数个数（**让缓冲区自描述**） |

**level 不进编码。** 它在编码前就用于过滤（运行期 `level > threshold` 直接 return），decode 时再由 map 按 `(file_id, line)` 还原。

U32 头之后紧跟 `param_count` 个 U32 参数。一条完整 entry = `4 + param_count*4` 字节。

```c
#define WW_LOG_ENCODE(file_id, line, pcnt) \
    ( (((U32)(file_id) & 0xFFF) << 20) | \
      (((U32)(line)    & 0x3FFF) << 6) | \
      ( (U32)(pcnt)    & 0x3F) )

#define WW_LOG_MODULE_OF(file_id)  (((file_id) >> 7) & 0x1F)
#define WW_LOG_OFFSET_OF(file_id)  ((file_id) & 0x7F)
```

> 与 v0 的差异：v0 是 `[file_id12][line12][datalen6][level2]`，line 只到 4095 且含 level。v1 去掉 level、line 扩到 16383、file_id 内部改为 5+7 划分。

### `%s` 处理（不禁止）

当前代码里 `%s` 主要用于打印 `__FILE__/__LINE__`，而 file/line 已在 U32 头里冗余存在。策略：
- 运行期：encode 模式照常把 `%s` 对应的指针当 U32 存入缓冲（浪费 4 字节，但无害）。
- decode 期：map 里的 fmt 含 `%s`，decoder 对该参数显示占位 `<%s@0xXXXXXXXX>`（内容不可还原）。
- 构建期扫描器遇到 `%s` **只告警不报错**。

---

## 3. 无感的 ID 管理 + 统一 JSON

### 输入：`log_config.json`（人工维护，只登记 模块→目录）

```json
{
  "modules": {
    "DEMO":    { "id": 1, "dirs": ["src/demo"],                 "enable": true },
    "DRIVERS": { "id": 4, "dirs": ["src/drivers", "src/hal"],   "enable": true },
    "TEST":    { "id": 2, "dirs": ["src/test"],                 "enable": false }
  },
  "unregistered": "warn"
}
```

- module 由 **路径前缀匹配** 判定，**最长前缀优先**。
- 一个模块可含多个目录、任意多文件。
- 不在任何 `dirs` 下的 .c → 告警 + 该文件 log 关闭（不阻断编译）。
- module_id 由人工在此分配，**稳定**（开关 key 在它上面）。范围 0–31。

### 生成：`tools/gen_log_map.py` 扫描 → `ww_log_map.json`（生成且提交）

脚本扫 `dirs` 下所有 `.c`，提取每个 `LOG_xxx(...)` 调用的 **行号 + level + fmt**，产出统一映射文件：

```json
{
  "meta": { "version": "<来自项目宏，先留空/注释>", "build_time": "...", "encoding": "file12_line14_pcnt6" },
  "modules": { "1": {"name":"DEMO","enable":true}, "4": {"name":"DRIVERS","enable":true} },
  "files":   { "64": {"path":"src/demo/demo_init.c","module":"DEMO"}, "65": {"path":"src/demo/demo_process.c","module":"DEMO"} },
  "entries": [
    {"file_id":64,"line":18,"level":"INF","fmt":"Demo module initializing..."},
    {"file_id":64,"line":26,"level":"INF","fmt":"Hardware check passed, code=%d"}
  ]
}
```

### file_id 锁定（关键，#2 决策）

- file_id = `module_id*128 + offset`（module_id 来自 config，offset 模块内分配）。
- **重新生成时先读旧 `ww_log_map.json`**：已分配过的文件保持原 offset 不变，新文件只在空位追加。删除的文件其 offset **保留占位、不回收**。
- 目的：旧固件的历史日志仍能被新 map 正确 decode（增删文件不让已存在 id 漂移）。

### 一文件两用（#3 决策）

`ww_log_map.json` 同时驱动构建和 decode：
- `gen_log_map.py --makefile` → 派生 `build/file_ids.mk`（供 Makefile `-D` 注入）。
- `gen_log_map.py --header` → 派生 `include/auto_file_ids.h`（模块/文件 ID 宏）。
- `tools/log_decoder.py --map ww_log_map.json` → 直接读它做还原。

### 构建期校验（#10）

扫描时统计 fmt 里非 `%%` 的占位符个数，与该调用实际传参个数比对，不一致 **告警**。

---

## 4. 开关设计（string / encode 都要）

两层，互不依赖：

### 静态开关（编译期，零代码）
Makefile 按文件注入 `CURRENT_MODULE_STATIC_EN`（来自该文件所属模块的 `enable`）。为 0 时 LOG 宏展开为空，**该文件 log 零体积**。沿用 v0 的 `_WW_LOG_IF(cond)` 拼接技巧。

另加编译期 level 阈值 `WW_LOG_COMPILE_THRESHOLD`：高于阈值的 `LOG_DBG/INF` 直接编译掉。

### 动态开关（运行期）
- `g_ww_log_module_mask`（U32，一位一个模块，0–31）→ `ww_log_set_module_mask / enable_module / disable_module`。
- `g_ww_log_level_threshold` → `ww_log_set_level_threshold`。
- 检查在输出函数内部完成（集中，减小调用点体积）：
  ```c
  if ((g_ww_log_module_mask & (1U << module_id)) == 0) return;
  if (level > g_ww_log_level_threshold) return;
  ```

> 开关 key 在 **module_id**（config 分配，稳定），不在 offset 上。因此 file_id 锁定/漂移不影响开关。动态开关停在**模块**粒度，不做文件粒度（避免依赖会变的 offset）。

### Makefile 注入（耦合点收敛）
日志核心只认三个注入宏：`CURRENT_FILE_ID` / `CURRENT_MODULE_ID` / `CURRENT_MODULE_STATIC_EN`。沿用 v0 的 per-file 编译规则（`$(eval ...)` 从 `file_ids.mk` 查这三个值）。这样将来换构建系统只需换"注入这三个宏"的方式，核心不动。

---

## 5. 输出后端（可组合，#6）

`ww_log_config.h` 里三个独立开关，可叠加：

```c
#define WW_LOG_BACKEND_UART     1
#define WW_LOG_BACKEND_RAM      1
#define WW_LOG_BACKEND_STORAGE  0
```

- 输出函数编码出 U32 后，依次分发给所有开启的后端（轻量函数指针表或直接 `#if` 串联，不引入动态注册复杂度）。
- **UART**：encode 模式输出 hex 帧（`0x%08X` 头 + 参数，沿用 v0 格式即可）；string 模式输出可读文本。
- **RAM**：环形缓冲，沿用 v0 `ww_log_ram.*`（4KB = 64B header + 数据区，3KB flush 阈值，热重启恢复）。
- **STORAGE**：flush 到外存，沿用 v0 `ww_log_storage.* / ww_log_header.* / ww_log_flush.*`。

调用 `LOG_xxx()` 的代码对后端组合完全无感。

---

## 6. panic 模式（#7，v0 没有）

```c
void ww_log_panic(void);
```

在 HardFault / watchdog 回调里调用，语义：
1. 绕过所有模块/level 过滤；
2. 立即同步 flush RAM → 外存（不等阈值）；
3. UART 切轮询输出（不依赖中断）；
4. 置 panic flag，后续 LOG 直接同步直写。

目的：没有 UART 时，靠 RAM/外存保住崩溃前最后几条日志。

---

## 7. 目录结构

```
ww_log/
├── CLAUDE.md                  ← 本文件
├── ww_log_v0/                 ← 旧实现，只读参考
└── ww_log_v1/                 ← 在这里写新代码
    ├── Makefile
    ├── log_config.json        ← 人工：模块→目录
    ├── ww_log_map.json        ← 生成（构建+decode 共用）
    ├── include/
    │   ├── type.h                  (从 v0 拷)
    │   ├── ww_log.h                (模式分发 + 公共 API + level 宏)
    │   ├── ww_log_config.h         (后端开关 / RAM 尺寸 / 阈值 / magic)
    │   ├── ww_log_encode.h
    │   ├── ww_log_str.h
    │   ├── ww_log_modules.h        (动态 mask + level 阈值 API)
    │   ├── ww_log_backend.h        (后端分发)
    │   ├── ww_log_ram.h
    │   ├── ww_log_storage.h
    │   ├── ww_log_header.h
    │   ├── ww_log_flush.h
    │   ├── ww_log_panic.h
    │   └── auto_file_ids.h         (生成)
    ├── core/
    │   ├── ww_log_common.c         (init / 全局变量定义)
    │   ├── ww_log_encode.c
    │   ├── ww_log_str.c
    │   ├── ww_log_modules.c
    │   ├── ww_log_backend.c
    │   ├── ww_log_ram.c
    │   ├── ww_log_storage.c
    │   ├── ww_log_header.c
    │   ├── ww_log_flush.c
    │   └── ww_log_panic.c
    ├── sim/                    ← PC 仿真外存（沿用 v0 sim_storage.*）
    ├── tools/
    │   ├── gen_log_map.py      (扫描 → ww_log_map.json / file_ids.mk / auto_file_ids.h)
    │   └── log_decoder.py      (--map ww_log_map.json 还原)
    ├── src/                    ← demo 模块，给 sim 跑通用（demo/ drivers/ test/）
    └── examples/
        └── main.c             ← 仿真主程序
```

实现顺序建议：type.h → config/modules（开关）→ encode → backend(UART) → str → ram/storage/flush → panic → gen_log_map.py → decoder → examples/main.c。

---

## 8. sim 验收标准（必须达成）

v1 用 **gcc 在 PC** 上编译运行（`SIMULATION_MODE`，外存用 `sim/` 静态数组模拟）。完成判据：

1. `cd ww_log_v1 && make && make run` 通过。
2. 三种模式都能切换并正确运行：
   - DISABLED：无 log 输出。
   - STR：输出 `[INF] demo_init.c:26 - Hardware check passed, code=0` 这类可读行。
   - ENCODE：输出 U32 hex 帧。
3. 动态开关生效：关某模块 / 调 level 阈值后，对应 log 不再输出。
4. encode + RAM + STORAGE：写入、达阈值 flush、热重启恢复路径都能跑。
5. **decode 闭环**：encode 模式的 hex 输出 → `log_decoder.py --map ww_log_map.json` → 还原出与 STR 模式一致的可读日志（含参数代入 fmt）。
6. `gen_log_map.py` 重跑两次（中间增删一个 .c）验证 **file_id 锁定**：已有文件 id 不变。

---

## 9. 约定

- 类型用 `type.h` 的 `U8/U16/U32`（从 v0 拷贝）。
- 命名沿用 v0：函数 `ww_log_<action>_<object>`，类型 `XXX_T/XXX_E`，配置宏 `WW_LOG_<FEATURE>`。
- 宏务必 `do{}while(0)` 包裹。
- 注释用 Doxygen 风格。
- 平台：RISC-V（Andes N25），裸机/轻 RTOS，无动态内存，RAM 紧张，UART 为主调试口。注意临界区（环形缓冲指针操作要原子）。
- 参考 v0 对应文件即可快速实现 ram/storage/flush/header，逻辑基本可复用，主要改动在编码格式（§2）和后端组合（§5）。
