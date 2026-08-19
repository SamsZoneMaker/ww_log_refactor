# 合入内网 FW 代码的顺序

对应 `ce5176c..HEAD` 的全部改动。每个阶段结束时代码树都应该是**可编译、可测**的，出问题就停在该阶段排查。

---

## 阶段 0：先做一件不可逆的准备

```bash
# 在合入任何东西之前，把 FW 当前的 map 归档一份
cp ww_log_map.json  maps/ww_log_map_<当前map_id>.json
```

**为什么必须先做**：阶段 1 会引入 offset 回收 —— 已删除文件占的槽位会被新文件复用。现网设备里如果还有用**旧 map** 编码的日志，只有那份旧 map 能正确还原它。合完再想存就晚了。

如果 FW 的 map 里还没有 `meta.map_id` 字段也没关系：map_id 是 map 内容的纯函数，用新版 `gen_log_map.py` 的 `compute_map_id()` 能对老文件重算出来。

---

## 阶段 1：工具链（不碰固件二进制，风险最低）

| 顺序 | 文件 | 说明 |
|---|---|---|
| 1 | `scripts/log/gen_log_map.py` | 整体替换。**只管 ID 和 map，没有配置渲染** —— `.conf → autoconf.h` 是仿真侧 `sim/conf_to_autoconf.py` 的事，不进固件 |
| 2 | `scripts/log/log_config.json` | 整体替换。只含模块划分，没有构建配置 |

**验证**：重跑生成器，检查 `ww_log_map.json` 的 `files` 段里**现存文件的 id 一个都没变**。

**预期的变化**（不是 bug）：
- `files` 多出 `short` 字段
- `entries` 多出 `N_RETURN_*_IF_TRUE` 的条目
- `meta` 多出 `map_id`
- 已删除文件的 `present: false` 占位条目**消失**

---

## 阶段 2：构建规则（增量编译）

不要直接抄 `Makefile`（那是仿真用的），要移植的是**三条规则**：

1. **拆时间戳** —— `file_ids.mk` 每次都 touch（它被 `-include`，保持旧 mtime 会让 GNU make 无限重启）；`auto_file_ids.h` 内容变才写，让目标文件依赖后者。
2. **write-if-changed** —— 所有生成物内容没变就不动 mtime（`gen_log_map.py --write` 已实现）。
3. **递归扫源码 + 目录也是依赖** —— 目录 mtime 会在增删文件时变化，否则**删除**一个源文件不会触发重新生成。

**验证**：改一个已登记 `.c` 只重编 1 个目标文件；空跑 0 个。

---

## 阶段 3：头文件（纯定义，无功能变化）

**必须按此顺序**，后者依赖前者的宏：

| 顺序 | 文件 | 内容 |
|---|---|---|
| 1 | `include/log/n_ww_log_def.h` | 控制记录命名空间、boot record 常量、阈值的 Kconfig 映射 |
| 2 | `include/log/n_ww_log_storage.h` | `LOG_FLAG_EXT_FULL`、旋钮 `#ifndef` 兜底、flush marker 改用命名空间、`#include n_ww_log_def.h`、EXT→RAM 的 `#error`、`log_ram_set_ext_full` 声明 |
| 3 | `include/log/n_ww_log_api.h` | boot record 两个函数声明 |

阶段 3 合完还编译不过是正常的（声明了但还没实现），下一阶段补上。

---

## 阶段 4：Kconfig

配置源就是你们现有的 Kconfig + `.conf → autoconf.h` 流程，**不需要任何额外的生成步骤**。`sim/log.conf` 是日志模块认识的全部符号的权威清单，可以直接和 defconfig 片段对照。

Kconfig 里需要动的只有两件事：

**① 一个真 bug —— `N_LOG_BACKEND_EXT_MEM` 必须依赖 RAM**

```diff
 config N_LOG_BACKEND_EXT_MEM
-    depends on N_LOG && N_LOG_MODE_ENCODE
+    depends on N_LOG && N_LOG_MODE_ENCODE && N_LOG_BACKEND_RAM
```

外存后端没有自己的存储，是从 RAM 环里搬。现在的 Kconfig 允许只选 EXT 不选 RAM，那个组合会在链接期挂掉；代码里已经加了 `#error` 明说要求，Kconfig 堵上是双保险。

**② 补上原本写死在头文件里的旋钮**（都有兜底默认值，不加也能编）

| 符号 | 类型 | 默认 |
|---|---|---|
| `N_LOG_COMPILE_THRESHOLD_{ERR,WRN,INF,DBG}` | choice | DBG |
| `N_LOG_EXT_LEVEL_THRESHOLD_{ERR,WRN,INF,DBG}` | choice | WRN |
| `N_LOG_EXT_FULL_{FREEZE,ERASE}` | choice | FREEZE |
| `N_LOG_EXT_FLUSH_MARKER` | bool | y |
| `N_LOG_RAM_FLUSH_THRESHOLD` | int | 480 |
| `N_LOG_EXT_FLUSH_STAGE_SIZE` | int | 256 |
| `N_LOG_WRITE_TIMEOUT_MS` | int | 6 |
| `N_LOG_FLUSH_TIMEOUT_MS` | int | 10000 |
| `N_LOG_FLUSH_TASK_STACK_SIZE` | int | 256 |
| `N_LOG_FLUSH_TASK_PRIORITY` | int | 1 |

等级用 `choice`（四个 bool）而不是 `int`：menuconfig 里显示 ERR/WRN/INF/DBG 更清楚，也不可能填越界；头文件里已有到 `N_WW_LOG_LEVEL_*` 的映射。

> mode 那个 `choice` 块**不用动** —— Kconfig 的 `choice` 本身就保证恰好选一个，这正是需要的性质。

**验证**：`menuconfig` 能正常选；旧 defconfig 不加任何新符号也能编。

---

## 阶段 5：固件核心 `.c`（真正改行为）

| 顺序 | 文件 | 内容 | 备注 |
|---|---|---|---|
| 1 | `log/n_ww_log_ram.c` | 整文件 `#ifdef CONFIG_N_LOG_BACKEND_RAM` 包裹、`log_ram_set_ext_full()`、满了不再 notify | |
| 2 | `log/n_ww_log_task.c` | 旋钮 `#ifndef` 兜底 | 无行为变化 |
| 3 | `log/n_ww_log_control.c` | boot record 写入 + `n_ww_log_init()` 里调用 | 需要 `log_map_id.h`（阶段 6）和 `version.h` |
| 4 | `log/n_ww_log_storage.c` | `ext_whole_entry_prefix()`、FREEZE 补尾、flush 的 boot lead | **和 3 相互依赖，必须一起合** |

3 和 4 之间有双向调用（storage.c 调 control.c 的 `n_ww_log_fill_boot_record`），分开合会断。

---

## 阶段 6：map_id 注入

生成 `output/log_map_id.h`（`gen_log_map.py --mapid --write`），**只被 `n_ww_log_control.c` include**。

> ⚠️ **绝不能用全局 `-D` 注入**。make 只比对文件时间戳、不跟踪命令行，map 变了而源文件没变时根本不重编，固件里会留一个过期的 map_id —— 而这个 id 的全部意义就是保证和 map 对得上。必须做成文件依赖。

同时确认 FW 的 `version.h` 里 `BUILD_VERSION` / `BUILD_GIT_ID` 宏名和 `n_ww_log_control.c` 里用的一致（不一致就改 control.c，那里有 `#ifndef` 兜底成 0）。

**验证**：UART 上第一条应该是 `0xFFFFFF83 <map_id> <version> <git_id>`。

---

## 阶段 7：自检套件（可选，强烈建议）

`src/test/test_log.c` —— 它只碰日志 RAM 区和 LOG 分区，可以在目标板上跑。需要 `log_map_id.h` 和 `version.h`。

**验证**：57 项全过。

---

## 阶段 8：host 工具（完全独立，任何时候都能合）

`host_driver/log_operation.py`、`log_tool.py`、`dora.py`

> ⚠️ **上硬件前必须核对 `log_operation.py` / `log_decoder.py` 里的四个分区表常量**：`PT_MAGIC`、`PT_ENTRY_TYPE_LOG`、`PT_HDR_FMT`、`PT_ENTRY_FMT`。现在照抄的是仿真占位定义。改这四个就够，布局没有编码在别处。

---

## 阶段 9：归档流程

把 `make map-archive` 的等价步骤挂进发版/打 tag 脚本，`maps/` 进版本控制。没有这一步，boot record 里的 map_id 只是一串没法兑现的号码。

---

## 不要合入的文件

| 文件 | 原因 |
|---|---|
| `sim/*` | 仿真专用的硬件壳 |
| `Makefile` | 仿真专用，只移植阶段 2 的规则 |
| `examples/main.c` | 仿真主程序 |
| `sim/log.conf`、`sim/conf_to_autoconf.py` | 仿真专用，FW 已有等价流程 |

---

## 三个单向门（合了就回不去）

1. **offset 回收** —— 已删除文件的槽位会被复用。合并前务必完成阶段 0。
2. **boot record** —— 外存归档每次启动多 16 字节。老归档仍能读（第一条 boot record 之前的 entry 会被 decoder 标为 map 未知）。
3. **map_id** —— 从生成的那一刻起，发版就必须归档 map，否则以后解不了那个版本的日志。

线上格式（entry 的 4 字节头布局）**没有变**，分区头**一个字节都没动**，所以现场设备的已有归档不会被擦。
