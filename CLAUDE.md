# CLAUDE.md — ww_log v2 实现规格书

> 本文件是 **ww_log_v2** 的实现规格与设计记录，`/clear` 之后照此继续。
> `ww_log_v0/`、`ww_log_v1/` 是历史实现，**只读参考，不要修改**。
> 新代码全部写在 `ww_log_v2/`。

---

## 0. 项目背景

嵌入式日志系统重构。痛点：string 模式在固件体积受限时太占空间。**encode 模式**把每条 log 压成定长二进制，大幅省 ROM/RAM；同时保留 string 模式（调试用）和全关模式。encode 数据写进掉电保持 RAM（4KB DLM 维护区），按需搬到外存 LOG 分区，PC 端用映射文件 decode 回可读日志。

平台：RISC-V（Andes N25），FreeRTOS，无动态内存，RAM 紧张，UART/JTAG 为主调试口。

---

## 1. 配置：两个正交的来源

**模式 / 后端 / 阈值 —— 走 Kconfig 符号**，两边用同一套符号名（完整清单见 `sim/log.conf`；固件侧的 Kconfig 由固件工程自己维护）：

| | 输入 | 转换 |
|---|---|---|
| 固件 | Kconfig `.conf` | 固件自己的 `.conf → autoconf.h` |
| 仿真 | `sim/log.conf`（同样的 `.conf` 语法、同样的符号名） | `sim/conf_to_autoconf.py` → `output/log_autoconf.h` |

所以一份 sim 配置和一份固件 defconfig 片段是**可互换的**，diff 一下就能比对。`sim/autoconf.h` 只是 include 生成结果的薄壳。

头文件里每个旋钮按优先级解析：**显式 `-D` / 生成的头 → `CONFIG_N_LOG_*` → 默认值**，所以什么都不选也能编。

仿真的 `.conf` 是手写的，`conf_to_autoconf.py` 补了两条 menuconfig 本来会给的校验：**恰好选一个 mode**（一个都不选会静默降级成 DISABLED，即出厂就是哑的），以及 **EXT_MEM 必须蕴含 RAM**。

**模块 → 目录 —— 走 `scripts/log/log_config.json`**：

```json
{
  "modules": {
    "DEMO":    { "id": 1, "dirs": ["src/demo"],               "enable": true },
    "TEST":    { "id": 2, "dirs": ["src/test"],               "enable": true },
    "DRIVERS": { "id": 4, "dirs": ["src/drivers", "src/hal"], "enable": true }
  },
  "unregistered": "warn"
}
```

这个文件**只管模块划分**，不含构建配置 —— 构建配置是 Kconfig 的职责，`gen_log_map.py` 也就只管 ID 和 map，因此能原样合进固件树，里面没有一行死代码。

模块由**路径前缀匹配**判定，最长前缀优先；不在任何 `dirs` 下的 `.c` → 告警 + 该文件日志关闭（不阻断编译）。module_id 人工分配、**稳定**（动态开关的 key 在它上面），范围 0–31。

调用点写法三种模式完全一致：
```c
N_LOG_ERR("msg");            N_LOG_WRN("x=%d", a);
N_LOG_INF("x=%d y=%d", a,b); N_LOG_DBG("...");
```

---

## 2. encode 编码格式（已锁定）

每条 entry = 4 字节头 + `pcnt` 个 U32 参数。

```
 31              20 19            6 5   4 3      0
┌──────────────────┬────────────────┬─────┬───────┐
│  file_id (12)    │   line (14)    │lv(2)│pcnt(4)│
└──────────────────┴────────────────┴─────┴───────┘
        └ file_id = [ module_id : 5 ][ offset : 7 ]
```

| 字段 | 位宽 | 说明 |
|---|---|---|
| file_id | 12 | 高 5 位 module_id(0–31)，低 7 位模块内 offset(0–127) |
| line | 14 | `__LINE__`，0–16383 |
| level | 2 | ERR/WRN/INF/DBG |
| pcnt | 4 | 后跟的 U32 参数个数，0–15（让缓冲区自描述） |

**level 进编码**（与 v1 规格不同）：flush 路径要能逐条判断该不该搬进外存，读头里的 level 即可，不需要旁表。代价是 pcnt 从 6 位缩到 4 位（最多 15 个参数）。

宏见 `n_ww_log_def.h`：`N_WW_LOG_ENCODE / _FILEID_OF / _LINE_OF / _LEVEL_OF / _PCNT_OF / _MODULE_OF / _OFFSET_OF`。

### 控制记录命名空间

日志模块写进自己数据流的记录。**`file_id = 0xFFF` 被生成器永久保留**，永不分配给源文件，所以不会和真实调用点撞车；它们的 `pcnt` 是正常的，所有遍历器（冷启动扫描、flush 打包、host 解码）都当普通 entry 跳过，**无需任何特判**。

| line | 记录 | 内容 | 大小 |
|---|---|---|---|
| `0x3FFF` | flush marker | `[hdr][tick]` | 8 B |
| `0x3FFE` | boot record | `[hdr][map_id][BUILD_VERSION][BUILD_GIT_ID]` | 16 B |
| `0x3FF0`–`0x3FFD` | 预留 | | |

### `%s` 处理

encode 模式照常把指针当 U32 存入（浪费 4 字节，无害）；decode 时显示 `<%s@0xXXXXXXXX>`；扫描器遇到 `%s` 只告警不报错。

---

## 3. ID 管理与 `ww_log_map.json`

`gen_log_map.py` 扫描 `modules.dirs` 下所有 `.c`，产出统一映射文件，**同时驱动构建和解码**。

### file_id 分配

- `file_id = module_id * 128 + offset`
- 重新生成时先读旧 map：**现存文件保持原 offset**（日常看裸 hex 时 id 稳定）
- **删除的文件其 offset 会被回收**

> 早期设计是「删除的 offset 永久占位」，用来让旧固件的日志能被新 map 解开。但它只冻结了 `file_id`，`line` 照样漂移 —— 而改任何一行代码都会让后面所有日志行号平移。结果是「文件名对、fmt 错」的**貌似合理但完全错误**的输出。跨版本解码改由 boot record + map_id 负责（§7），占位就只剩浪费槽位了。

### 扫描覆盖

除了字面 `N_LOG_xxx(`，还识别这些**展开成 `N_LOG_ERR` 的 helper 宏**（`n_ww_log_macro.h`）：

```
N_RETURN_CODE_IF_TRUE / N_RETURN_IF_TRUE  ->  "-- line:%d rc:0x%x\r\n", 2 参数
N_BREAK_IF_TRUE / N_CONTINUE_IF_TRUE / N_PRINT_IF_TRUE  ->  "rc:0x%x\r\n", 1 参数
```

宏体里的 `__LINE__` 在**调用点**展开（GCC 对多行调用取起始行，与扫描器记录一致），所以它们发出的是归属正确的真实 entry；不进 map 的话每一条都解成 `<no map entry>`。`_WO_PRINT` 变体不发日志，故意不在表内。

**头文件里打日志无解**：一行 `.h` 代码被 N 个 `.c` include 就产生 N 个不同 `file_id`，且行号可能和该 `.c` 自己的日志撞车，`(file_id, line)` 表达不了。扫描器只告警，不生成会解错的 entry。

### 显示名消歧

`target/a/main.c` 和 `target/b/main.c` 这类重名：ID 分配本来就按路径、没问题，问题在显示层。生成器算一份显示名存进 map 的 `short` 字段，**STR 模式（`__NOTDIR_FILE__`）和 decoder 共用**：basename 唯一就用 basename（输出不变），重名才带目录。

### 构建期校验

fmt 里非 `%%` 的占位符个数 vs 实际传参个数，不一致告警；同一行出现两处日志调用会告警（`(file_id, line)` 无法区分）。

---

## 4. 开关

### 静态（编译期，零体积）
Makefile 按文件注入 `CURRENT_FILE_ID` / `CURRENT_MODULE_ID` / `CURRENT_MODULE_STATIC_EN`（来自 `file_ids.mk`）。`STATIC_EN=0` 时宏展开为空，该文件日志零体积。另有编译期阈值 `N_WW_LOG_COMPILE_THRESHOLD`。

**日志核心只认这三个注入宏**，换构建系统只需换注入方式，核心不动。

### 动态（运行期）
`g_ww_log_module_mask`（U32，一位一模块）+ `g_ww_log_level_threshold`：

```c
if ((g_ww_log_module_mask & (1U << module_id)) == 0) return;
if (level > g_ww_log_level_threshold) return;
```

检查集中在输出函数内部以减小调用点体积。开关 key 在 **module_id** 上，不做文件粒度（避免依赖会变的 offset）。

---

## 5. 输出后端（可组合）

`ww_log_backend_emit()` 把编码后的 entry 扇出给所有开启的后端：

- **UART** — hex 帧 `0x%08X ...`（string 模式则是可读文本）
- **RAM** — 4KB DLM 维护区环形缓冲，热重启保留
- **EXT_MEM** — 外存 LOG 分区（依赖 RAM 后端）

`ww_log_backend_emit()` **不做任何过滤**，是 boot record 这类必达记录的入口；带过滤的入口是 `n_ww_log_encode_output()`。调用 `N_LOG_*` 的代码对后端组合完全无感。

---

## 6. 外存归档（log-structured）

LOG 分区是**追加流**，不是定长槽位环：

```
[log_offset .. +8)          8B 'XLOG' 分区头，首次初始化写一次，此后不再重写
[log_offset+8 .. write_off) 背靠背的完整 entry（和 RAM 环里的线格式完全相同）
[write_off .. 分区末)        0xFF 未写区
```

- 分区 offset/size **来自分区表**（`pt_entry_get_by_key(pt, PART_ENTRY_TYPE_LOG, ...)`），不写死
- 每次 flush 把通过 `N_WW_LOG_EXT_LEVEL_THRESHOLD` 的完整 entry 追加到 `write_off`；无块头、无 CRC、无 footer —— 流自描述（pcnt 给出长度），首个 `0xFFFFFFFF` 即结尾
- `write_off` 在 RAM 常驻 ctx（noinit）：热重启保留；**冷启动靠扫描重建**，从而跨重启保留历史归档
- 为什么不用定长槽位：NOR 无法擦子扇区；且定时 flush 几个字节会浪费整个 512B 槽

### 写满策略

`freeze`（默认）停止追加、保留最早的日志；`erase` 擦掉重来、保留最新的。追加流没有逐槽滚动窗口，只能二选一。

FREEZE 触发时：
- **按 entry 边界把分区尾部填满**（不整批丢弃，否则最多浪费一个 staging buffer ≈255 B）；追加流绝不能停在半条 entry 上
- 置 `LOG_FLAG_EXT_FULL`（RAM 头 bit4）—— ext ctx 冷启动会丢，而 RAM 头会进 dump，否则「归档中途停止」和「设备不再打日志」无法区分
- 之后不再唤醒 flush task（醒来只会立刻退出）

---

## 7. 跨固件版本解码（核心机制）

归档刻意跨重启和**固件升级**保留，所以一条流里会有多个不同 map 产生的 entry。用当前 map 解全部是最危险的情况：旧的 `(file_id, line)` 在新 map 里通常仍能命中**某一条** entry —— 只是那一行现在是另一条语句 —— 输出看着合理但是错的。

### map_id

对**影响解码结果**的东西取 sha256 前 4 字节：encode 格式标签 + 每个 `(file_id → path)` + 每个 `(file_id, line, level, fmt)`。

刻意**不含**：`meta`（build_time、map_id 自身）、模块 enable 开关、JSON 排版、派生的 `short` —— 这些变了解码结果不变，纳入只会制造假告警。

两个性质：
1. **是 map 文件内容的纯函数** → 这个字段出现之前生成的老 map 也能重算出 id 并被正确索引；`meta.map_id` 只是交叉校验
2. canonical 序列化格式**已冻结**，改了会给所有归档 map 重新贴标签

落在 `0x00000000` / `0xFFFFFFFF` 时强制改成 `1`（这两个值在流里像"未初始化"和"已擦除"）。

### boot record

固件**不做任何版本比较**，只在流里盖戳。`n_ww_log_init()` 通过 `ww_log_backend_emit()`（不过滤）发一条 16B boot record，level 取 ERR 保证一定过外存阈值。它同时出现在 UART、RAM 环和外存。

**不变量**：归档里只要有 entry，前面一定有 boot record。这由 **flush 路径**保证 —— 一个空归档收到第一批数据时自动补一条，而不是依赖 init 时序（"擦除归档 → 随后清空 RAM 环"会在盖的戳被 flush 之前就丢掉它）。

顺序在两种启动下都对：冷启动时环已清空，boot record 是本次启动第一条；热重启时未 flush 完的残留 entry 排在它前面，flush 按最旧优先，所以那些残留仍归属**上一条** boot record。

### 分区头一个字节都不动

原本想把 map_id 塞进 `LOG_EXT_PART_HDR_T` 的 `reserved`，**放弃了**：`log_ext_mem_init()` 的续接检查是 `magic != ... || version != LOG_EXT_FORMAT_VERSION`，一旦改结构、版本号从 1 变 2，所有现场设备升到这版固件的瞬间归档会被**全擦** —— 正好是这个功能要防的事。保持格式版本 1，老归档原样保留，第一条 boot record 之前的 entry 由 decoder 明确标注为"map 未知"。

### map 归档

boot record 只写下一个 id，**那份 map 文件得有人存着**。开发期每改一行 map_id 就变，自动存会瞬间堆出几百个一次性文件，所以做成显式一步：

```bash
make map-archive     # -> maps/ww_log_map_<map_id>.json
```

发版/打 tag 时调用（建议挂进 CI）。归档时顺带从 `version.h` 读出 `BUILD_VERSION` / `BUILD_GIT_ID` / `BUILD_TIME` 写进 meta（用 `fw_` 前缀，避免和 map 自己的 `build_time` 撞名），这样每份归档 map 自带"我属于哪个固件版本"。`maps/` 进 git。

### 解码

```bash
python3 log_decoder.py --map ww_log_map.json --map-dir maps/ dump.bin
log_tool.py eeprom --map ww_log_map.json --map-dir maps/      # JTAG 侧同样支持
```

decoder 遇到 boot record 就切换当前 map：

| 情况 | 行为 |
|---|---|
| id 命中 | 打印段头，正常解 |
| id 未命中 | 段头带 WARNING，用默认 map 解但**每行前缀 `?`** |
| 第一条 boot record 之前的 entry（老归档） | 同样 `?` |

---

## 8. 构建（增量编译是硬约束）

改一个源文件只会移动**行号**（只有 decoder 关心，走 `ww_log_map.json`），不会改变 **file_id**（`-D` 注入，影响每个目标文件）。这两者**绝不能共用一个时间戳**，否则每次保存都要全量重编。

| 生成物 | 时间戳策略 | 谁依赖它 |
|---|---|---|
| `ww_log_map.json` | 内容变才写 | 无（只给 decoder） |
| `file_ids.mk` | **每次都 touch** | 无（只被 `-include`） |
| `auto_file_ids.h` | 内容变才写 | 所有 `.o` |
| `log_map_id.h` | 内容变才写 | 只有 control.c / test_log.c |
| `log_autoconf.h` | 内容变才写 | 所有 TU（被强制 include） |

> `file_ids.mk` 必须每次 touch：它被 `-include`，而一个目标如果保持比依赖更旧的 mtime，**GNU make 会无限重启**。所以让它照常 touch，把"内容没变就别动"的责任交给 `auto_file_ids.h`。

实测（12 个目标文件）：

| 操作 | 重编译数 |
|---|---|
| 改一个已登记 `.c`（map_id 不变） | 1 |
| 改一个已登记 `.c`（行号平移，map_id 变） | 3 |
| 新增 / 删除 `.c` | 全部（保守且正确） |
| 空跑 | 0 |

其它两点：
- `$(LOG_SRCS)` 用递归 `find`，和生成器的 `os.walk` 一致 —— 非递归 wildcard 会给 `src/demo/sub/x.c` 发 file_id 却永远不编译它
- 扫描目录本身也是依赖：**删除**源文件会让它从 `$(LOG_SRCS)` 里消失，就没有比 `file_ids.mk` 更新的东西了，map 会一直标它存在；目录 mtime 会在增删条目时变化，正好覆盖这种情况

**`map_id` 绝不能用全局 `-D` 注入** —— make 只比对文件时间戳、不跟踪命令行，map 变了而源文件没变时它根本不重编，固件里会留一个过期的 map_id，而这个 id 的全部意义就是保证对得上。所以做成文件依赖：生成 `output/log_map_id.h`，只被需要它的 `.c` include。

---

## 9. host 工具

`host_driver/` 是 dora 部署里的自包含副本（不依赖 `scripts/`）；`scripts/log/log_decoder.py` 是离线版。两边逻辑保持同步。

**分区几何来自分区表**，不写死：`log_tool.py flash|eeprom` 默认从设备分区表读 LOG 分区的 offset/size（固件用的是同一张表），`--offset/--length` 仅作覆盖。离线 decoder 拿到整片镜像时同样先找分区表，把搜索范围限定到 LOG 分区。

> ⚠️ `PT_MAGIC` / `PT_ENTRY_TYPE_LOG` / 结构体布局目前照抄 `sim/init_ex.h`，那是仿真用的**占位定义**（`PART_ENTRY_TYPE_LOG (8)` 上面就挂着 TODO）。**上硬件前必须和真实固件核对这四个常量**；改了它们下游全部自动跟上，布局没有编码在别处。

sim 侧的分区表现在**写在模拟设备的 offset 0**（不是在 RAM 里现造），`sim_ext_dump_chip()` 能导出整片镜像，所以 host 的分区表解析路径在仿真里就能跑通。

---

## 10. panic 模式（TODO — 待固件集成，当前已删除）

> panic 相关代码已从 v1/v2 删除。下一步研究固件向量中断时按此清单接回来，其中 **#1** 是最直接的挂载点。

panic 不是主动调用的功能，而是挂在**系统异常入口**上：崩溃瞬间抢救崩溃前的日志（①绕过过滤 ②同步 flush RAM→外存 ③UART 切轮询 ④置 flag 后续直写）。

### #1 主战场：RISC-V trap / 异常处理函数

RISC-V 同步异常都跳到 `mtvec` 指向的 trap 入口，handler 读 `mcause` 判因。BSP/startup 里那个函数（常见名 `trap_handler` / `trap_entry` / `default_exception_handler`，在 `trap.c` 或 `startup_*.S`）就是挂载点。对**不可恢复异常**：

```c
void trap_handler(unsigned long mcause, unsigned long mepc)
{
    if ((mcause >> (__riscv_xlen - 1)) == 0) {        /* 最高位=0 → 异常 */
        switch (mcause & 0xFF) {
            case 1: case 2: case 5: case 7:           /* access / illegal fault */
            case 0: case 4: case 6:                   /* misaligned */
                N_LOG_ERR("FATAL trap mcause=%d mepc=%x", (int)mcause, (unsigned)mepc);
                ww_log_panic();        /* N_LOG_ERR 必须在 panic 之前 */
                ww_system_reset();
                break;
            default: break;
        }
    }
}
```

崩溃现场最该记 `mcause`/`mepc`（出错指令地址）和核心寄存器。

### #2 看门狗
有预超时 / window 中断才接得上；无预警硬复位时靠"日志平时就在掉电保持 RAM 区"，复位后 `n_ww_log_init()`（内部 `log_ram_init(force_clear=0)`）捞回来。

### #3 软件致命路径
`ASSERT()` 失败分支、栈溢出 / 内存分配失败 hook、任何 `while(1)` 死等前。

### ⚠️ 三个坑
1. panic 跑在异常上下文，中断可能是关的 → UART 输出必须**纯轮询**（busy-wait FIFO），不能依赖 TX 中断/DMA
2. panic 里同步 flush 外存**只在驱动能在异常上下文同步跑时才安全**；要等中断/信号量会死锁 → 那就只保 RAM
3. 掉电保持 RAM 区必须落在**不被启动代码清零的段**（noinit / `.no_init`），否则复位即丢

### 闭环
业务运行 → trap 里 panic 保命 → 复位 → `n_ww_log_init()` 恢复 → decode 看现场。

---

## 11. 目录结构

```
ww_log_v2/
├── Makefile
├── ww_log_map.json          ← 生成并提交（构建 + 解码共用）
├── maps/                    ← 发版 map 归档，按 map_id 命名（make map-archive）
├── scripts/log/
│   ├── log_config.json      ← 模块→目录（不含构建配置）
│   ├── gen_log_map.py       ← 扫描 → map / file_ids.mk / auto_file_ids.h
│   │                          / log_map_id.h / 归档
│   └── log_decoder.py       ← 离线解码（--map / --map-dir）
├── include/log/
│   ├── n_ww_log.h           ← 唯一公共入口
│   ├── n_ww_log_def.h       ← 等级/阈值/编码位域/控制记录（最底层，无依赖）
│   ├── n_ww_log_api.h       ← init + 运行期开关 + boot record
│   ├── n_ww_log_output.h    ← 输出函数声明
│   ├── n_ww_log_macro.h     ← N_LOG_* 与 N_*_IF_TRUE
│   ├── n_ww_log_storage.h   ← RAM 环 + 外存几何
│   └── n_ww_log_task.h
├── log/                     ← 固件核心（control / output / ram / storage / task）
├── MERGE_TO_FW.md           ← 合入内网 FW 的分阶段清单
├── sim/                     ← PC 仿真硬件壳（DLM RAM、flash/eeprom、分区表、version.h）
│   ├── log.conf             ← 仿真配置，Kconfig .conf 语法
│   └── conf_to_autoconf.py  ← .conf → autoconf.h（仿真专用，不进固件）
├── src/                     ← demo / drivers / test 三个被登记的模块
├── examples/main.c
├── host_driver/             ← dora 部署（log_operation.py / log_tool.py / dora.py）
└── output/                  ← 全部生成物（.gitignore）
```

头文件分层是**无环**的：`def ← output ← macro`，`def ← api`，没有头文件 include 它的下游。

---

## 12. 验收标准

1. `cd ww_log_v2 && make && make run` 通过
2. **mode × backends 矩阵全绿**：3 种模式 × 4 种后端组合 = 12 项全部编译通过、零 FAIL
3. 自检套件 `src/test/test_log.c`：**57 passed / 0 failed**（encode + 全后端）
4. 动态开关生效（关模块 / 调 level 阈值后对应日志不再输出）
5. encode + RAM + EXT：写入、达阈值 flush、热重启恢复、冷启动扫描重建 `write_off`
6. **decode 闭环**：encode hex → decoder → 与 STR 模式逐字一致
7. **跨版本闭环**：两个行号不同的构建产生的混合流，只给当前 map 时旧段被标记并显示 `<no map entry>`，给 `--map-dir` 时两段各用自己的 map 全部解对
8. **增量编译**：改一个已登记 `.c` 只重编 1–3 个目标文件，空跑 0 个
9. **整片镜像解码**：`ext_chip.bin`（含分区表）→ 自动定位 LOG 分区 → boot record 归属 → 正确解码

---

## 13. 约定

- 类型用 `ww_type.h` 的 `U8/U16/U32`
- 命名：函数 `n_ww_log_<action>_<object>` / `log_<action>_<object>`，类型 `XXX_T/XXX_E`，配置宏 `CONFIG_N_LOG_*` 与 `N_WW_LOG_*`
- 宏务必 `do{}while(0)` 包裹
- 注释 Doxygen 风格；**注释解释"为什么"，不复述代码**
- 注意临界区（环形缓冲指针操作要原子）
- 改了线上格式/编码，`ENCODING_TAG` 和 `compute_map_id` 的 canonical 序列化都要同步考虑

---

## 14. 已知遗留

- **每模块 128 个 offset 的上限**：目前够用；真要突破得改位宽划分（如 4+8 = 16 模块 × 256 文件），那是编码格式变更，decoder 和已有归档 map 都要同步
- **扫描器不剥注释**：被注释掉的 `N_LOG_xxx(...)` 仍会进 map。是无害的冗余 entry（永远不会被发出），但会污染 map
- **`PART_TABLE_T` 布局待和真实固件核对**（见 §9）
- **panic 未接入**（见 §10）
