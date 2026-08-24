# ww_log v2 近期更新说明

本文以 Git 提交 `484541271a2b` 为旧版本基线，以 `9c803e9` 为本次整理时的功能版本，说明当前 `d_main` 相对基线新增或重构的功能，既包含使用方法，也包含关键实现原理。

本文侧重“功能为何存在、各部分怎样配合”。实际向内网 FW 搬运文件时，另请配合 [MERGE_TO_FW.md](MERGE_TO_FW.md) 使用。

## 1. 更新概览

| 主题 | 基线版本 | 当前版本 | 主要收益 |
| --- | --- | --- | --- |
| 编码头 | `file_id(12) + line(14) + pcnt(6)` | `file_id(12) + line(14) + level(2) + pcnt(4)` | 外存搬运时可以按每条日志的 level 过滤 |
| 最大参数数 | 16 | 15 | 让出 2 bit 保存 level |
| 外存格式 | `LOGH` 固定块 + `FLOG` footer | `XLOG` 分区头 + 连续追加的 entry stream | 不再为小批量 flush 支付整块开销，直接按实际分区大小使用 |
| 外存位置和容量 | host 侧存在固定值 | FW 和 host 均从 partition table 读取 LOG offset/size | 分区调整后无需同步修改硬编码大小 |
| 跨 FW 版本解码 | 只能人工选择一个 map | boot record 写入 `map_id/version/git_id`，decoder 按段切换 map | 同一个外存归档可包含多个 FW 版本 |
| file ID 管理 | 删除文件后永久保留 ID | 现存文件保持 ID，删除文件的 ID 可以复用 | 避免一个 module 的 127 个 offset 被历史文件耗尽 |
| 同名源文件 | 多个 `main.c` 都显示成 `main.c` | 使用最短唯一尾路径，例如 `branch_a/main.c` | string 和 encode 输出均可区分同名文件 |
| helper 宏扫描 | 只识别直接 `N_LOG_*` | 识别 `N_RETURN_*` 等会展开成日志的 helper | helper 调用可以按调用处的文件和行号正确解码 |
| 头文件扫描 | 每次构建都扫描并 warning | 默认关闭，通过 `--check-headers` 定期检查 | 正常构建更安静，仍保留开发检查能力 |
| 配置方式 | choice/头文件默认值混合 | `.conf` 使用数值策略，backend 使用 bool | FW、仿真和 C 条件判断使用同一组符号 |
| 增量构建 | map 更新可能触发全部对象重编 | map、file ID header 和 map ID header 分离依赖 | 普通源文件编辑只重编必要对象 |

当前 encode 数据链路如下：

```text
N_LOG_xxx 调用
    |
    +-- 编译期阈值 / module 静态开关
    |
    v
4B encoded header + 0..15 个 U32 参数
    |
    +-- UART：输出十六进制 frame
    |
    +-- RAM：保留所有通过 runtime threshold 的日志
             |
             +-- flush task：按 ext threshold 筛选
                    |
                    v
             外存 LOG partition：XLOG + 追加式 entry stream
                    |
                    v
        log_decoder.py / JTAG log_tool.py
                    |
                    +-- 遇到 boot record 后按 map_id 切换历史 map
```

## 2. 使用方法

### 2.1 `.conf` 配置

推荐配置如下：

```text
CONFIG_N_LOG=y

# mode: string=1, encode=2, disable=3
CONFIG_N_LOG_MODE=2

# backend 使用 Kconfig bool
CONFIG_N_LOG_BACKEND_UART=y
CONFIG_N_LOG_BACKEND_RAM=y
CONFIG_N_LOG_BACKEND_EXT_MEM=y

# level: error=0, warning=1, info=2, debug=3
CONFIG_N_LOG_COMPILE_THRESHOLD=3
CONFIG_N_LOG_RUNTIME_THRESHOLD=3
CONFIG_N_LOG_EXT_LEVEL_THRESHOLD=1

# ext full: freeze=1, erase=2
CONFIG_N_LOG_EXT_FULL=1

# 每次 flush-task 唤醒后的首个有效批次写入 tick marker
CONFIG_N_LOG_EXT_FLUSH_MARKER=y

CONFIG_N_LOG_FLUSH_TIMEOUT_MS=10000
```

各阈值的作用位置不同：

- `CONFIG_N_LOG_COMPILE_THRESHOLD`：预处理阶段裁剪，高于阈值的 `N_LOG_*` 不进入镜像。
- `CONFIG_N_LOG_RUNTIME_THRESHOLD`：系统启动时赋给全局运行时阈值；之后仍可通过 `n_ww_log_set_level_threshold()` 修改。
- `CONFIG_N_LOG_EXT_LEVEL_THRESHOLD`：RAM 搬外存时使用。RAM 仍保存所有通过 runtime threshold 的日志，外存只保存 `level <= ext threshold` 的日志。

配置依赖为：

- RAM 和 EXT_MEM 只支持 encode 模式。
- EXT_MEM 必须同时使能 RAM，因为外存后端是从 RAM ring 搬运，而不是直接接收调用点日志。
- disable 模式不能使能任何 backend。
- Kconfig 负责菜单约束，`n_ww_log_def.h` 还会再做一层编译期检查，防止手写 `.conf` 绕过约束。

固定的实现参数没有继续放进 `.conf`：

- `LOG_RAM_FLUSH_THRESHOLD`、`LOG_EXT_FLUSH_STAGE_SIZE` 位于 `include/log/n_ww_log_storage.h`。
- `LOG_WRITE_TIMEOUT_MS`、task stack 和 priority 位于 `include/log/n_ww_log_task.h`。

### 2.2 注册 module 和扫描目录

`scripts/log/log_config.json` 只负责 module、module ID、扫描目录和静态使能状态，不再负责生成 Kconfig 配置：

```json
{
  "modules": {
    "DEMO": {
      "id": 1,
      "dirs": ["src/demo"],
      "enable": true
    }
  },
  "unregistered": "warn"
}
```

规则如下：

- module ID 范围是 `0..31`。
- module `0..30` 各有 128 个普通 file offset；module 31 的最后一个 offset 需要组成保留值 `0xFFF`，因此只有 127 个普通 slot。
- `file_id = module_id * 128 + offset`。
- 目录会递归扫描；重叠目录采用最长路径前缀匹配。
- file ID 以完整相对路径为 key，因此多个分支目录中同时存在 `main.c` 不会发生 ID 冲突。
- 展示名称默认是 basename；basename 重复时自动扩展为最短唯一尾路径。
- `file_id=0xFFF` 为控制记录保留，生成器不会分配给真实源文件。

### 2.3 日志调用约束

普通调用方式不变：

```c
N_LOG_ERR("open failed, rc=0x%x", rc);
N_LOG_WRN("retry=%u", retry);
N_LOG_INF("link up");
N_LOG_DBG("state=%u flags=0x%x", state, flags);
```

encode 模式下建议遵守以下约束：

- 参数按 U32 存储，单条日志最多 15 个参数。
- `%s` 只能保存指针值，host 无法从离线 dump 恢复指针所指字符串。
- 一行源代码不要放两个会产生日志的宏；二者会得到相同 `(file_id, line)`，map 无法区分。
- 不建议在格式字符串末尾添加 `\n`/`\r\n`，换行应由输出层或 decoder 负责。
- `N_RETURN_CODE_IF_TRUE`、`N_RETURN_IF_TRUE`、`N_BREAK_IF_TRUE`、`N_CONTINUE_IF_TRUE` 和 `N_PRINT_IF_TRUE` 已被生成器识别；它们生成的日志归属 helper 的调用行。
- `_WO_PRINT` helper 不产生日志，也不会进入 map。

### 2.4 生成 map 和构建派生产物

生成器现在有四类核心输出：

| 产物 | 作用 | 何时应更新 |
| --- | --- | --- |
| `ww_log_map.json` | file/path/line/level/fmt 映射和 `map_id` | 日志调用或源文件变化时 |
| `file_ids.mk` | 每个源文件的 file ID、module ID、静态开关和短文件名 | 每次扫描均可重建；不应成为所有对象的直接依赖 |
| `auto_file_ids.h` | module/file ID 宏 | ID 内容真的变化时 |
| `log_map_id.h` | `N_WW_LOG_MAP_ID` | map 的解码相关内容变化时 |

命令顺序必须先生成 map，再从已经存在的 map 派生其他文件：

```sh
python3 scripts/log/gen_log_map.py scripts/log/log_config.json \
    --root "$(pwd)" --out ww_log_map.json

python3 scripts/log/gen_log_map.py scripts/log/log_config.json \
    --root "$(pwd)" --out ww_log_map.json --makefile > output/file_ids.mk

python3 scripts/log/gen_log_map.py scripts/log/log_config.json \
    --root "$(pwd)" --out ww_log_map.json \
    --header --write output/auto_file_ids.h

python3 scripts/log/gen_log_map.py scripts/log/log_config.json \
    --root "$(pwd)" --out ww_log_map.json \
    --mapid --write output/log_map_id.h
```

如果内网 Makefile 继续采用 `.new + rsync`，第一条 map 生成命令不要直接把 `--out` 指向一个每次都会删除的 `.new` 文件。生成器需要读取旧的 `ww_log_map.json`，才能让仍然存在的源文件保持原 file ID。可以直接输出正式 map，因为脚本本身已经实现 write-if-changed；或者先把旧 map 复制到 `.new` 后再生成。

相较旧的 `generate_log_cfg`，必须补充 `--mapid` 输出，并保证：

- `n_ww_log_control.c` 对应对象依赖 `log_map_id.h`。
- 普通注册源文件对象依赖 `auto_file_ids.h`，不要依赖每次都会刷新的 `file_ids.mk`。
- 源文件目录本身也应作为扫描依赖，否则删除 `.c` 后可能没有任何 prerequisite 变新。
- Makefile 的源文件发现方式应与生成器一致地递归扫描。
- string 模式编译时要把 `SHORT_NAME_*` 注入 `__NOTDIR_FILE__`，否则同名 `main.c` 又会退化为相同显示名。

本仓库 `Makefile` 展示了完整依赖关系，但它是 PC 仿真 Makefile，不应整体搬入 FW。

### 2.5 头文件日志检查

正常生成默认只扫描 `.c`：

```sh
python3 scripts/log/gen_log_map.py scripts/log/log_config.json \
    --root "$(pwd)" --out ww_log_map.json
```

开发阶段或发布前检查头文件：

```sh
python3 scripts/log/gen_log_map.py scripts/log/log_config.json \
    --root "$(pwd)" --out ww_log_map.json --check-headers
```

头文件中的可执行/static-inline 日志无法可靠生成唯一映射：同一个头文件行可能被多个 `.c` 展开，每个 includer 的 file ID 不同，也可能与 `.c` 自身相同行号冲突。因此该选项只报告 warning，不把头文件日志写进 map。

`n_ww_log_macro.h` 保存的是宏定义而不是调用点，已从该检查中整体排除。不要在这个文件中新增真正执行日志的 static-inline 函数，否则检查器也会跳过它。

### 2.6 发布时归档 map

每次正式发布都应归档当前 map：

```sh
python3 scripts/log/gen_log_map.py scripts/log/log_config.json \
    --root "$(pwd)" --out ww_log_map.json \
    --archive maps --version-header path/to/version.h
```

本地仿真也可以使用：

```sh
make map-archive
```

生成文件名类似：

```text
maps/ww_log_map_86935775.json
```

归档不应放进每次普通编译。开发中任意日志行号变化都可能产生新的 map ID，自动归档会生成大量没有发布价值的 map。

### 2.7 离线解码

UART 十六进制日志：

```sh
python3 scripts/log/log_decoder.py \
    --map ww_log_map.json capture.txt
```

RAM、LOG 分区或整片存储 bin：

```sh
python3 scripts/log/log_decoder.py \
    --map ww_log_map.json --map-dir maps dump.bin
```

常用选项：

- `--format auto|hex|bin`：覆盖自动格式识别。
- `--raw`：在可读日志后追加原始 frame。
- `-o result.txt`：保存解码文本。
- `-o result.bin` 或 `.dump`：保存原始二进制。
- `--hex "0x... 0x..."`：直接解码命令行中的单条或多条 frame。

对整片 flash/EEPROM bin，decoder 会先寻找 partition table，从 LOG entry 读取真实 offset/size，再在该窗口中寻找 `XLOG`。如果没有 partition table，则回退为寻找 `WLOG/XLOG` magic 或把输入当作裸 entry stream。

### 2.8 通过 JTAG 直接读取

集成到完整 DORA 工具目录后，可以直接读取设备：

```sh
python host_driver/log_tool.py ram \
    --map ww_log_map.json --map-dir maps

python host_driver/log_tool.py flash \
    --map ww_log_map.json --map-dir maps

python host_driver/log_tool.py eeprom \
    --map ww_log_map.json --map-dir maps --dev-addr 0x57
```

flash/EEPROM 默认从设备 partition table 获取 LOG offset 和 size；`--offset`、`--length` 只用于显式覆盖。`--hex` 可只查看原始字，`--save` 自动生成时间戳文件，`-o` 可指定输出文件。

本仓库只保存了 host 侧改动文件，不包含完整 DORA 的 `api/config.py` 运行环境，因此这些命令需要放回内网完整工具树后使用。

## 3. 关键实现原理

### 3.1 新编码头和三级阈值

当前 32-bit header：

```text
31             20 19         6 5   4 3      0
+-----------------+------------+-----+--------+
|   file_id (12)  |  line (14) |lv(2)|pcnt(4)|
+-----------------+------------+-----+--------+
```

`level` 进入 wire format 后，RAM entry 本身就带等级。数据路径因此可以分成三层：

1. 编译期：宏根据 compile threshold 决定是否生成调用。
2. 运行期：`n_ww_log_encode_output()` 根据 module mask 和 runtime threshold 决定是否发往 backend。
3. 搬运期：`log_ram_pack_ext()` 从 header 取出 level，只把满足 ext threshold 的完整 entry 放入 staging buffer。

被 ext threshold 过滤掉的 entry 仍会计入 `consumed` 并推进 RAM `read_index`，因此低优先级日志不会卡住外存搬运游标。

### 3.2 控制记录命名空间

日志模块内部生成的记录也使用普通 entry 结构，只占用保留的 `(file_id, line)`：

| 控制记录 | file ID | line | 结构 | 大小 |
| --- | --- | --- | --- | --- |
| flush marker | `0xFFF` | `0x3FFF` | `[header][tick]` | 8 B |
| boot record | `0xFFF` | `0x3FFE` | `[header][map_id][BUILD_VERSION][BUILD_GIT_ID]` | 16 B |
| 预留区 | `0xFFF` | `0x3FF0..0x3FFD` | 未来扩展 | - |

代码中的宏对应关系：

- `N_WW_LOG_CTRL_FILE_ID`：控制记录专用 file ID，值为 `0xFFF`。
- `N_WW_LOG_CTRL_LINE_FLUSH`：flush marker 的 line，值为 `0x3FFF`。
- `N_WW_LOG_CTRL_LINE_BOOT`：boot record 的 line，值为 `0x3FFE`。
- `N_WW_LOG_BOOT_RECORD_PCNT`：boot record 参数数，当前为 3。
- `N_WW_LOG_BOOT_RECORD_HDR`：由控制 file ID、boot line、ERR level 和 pcnt 编出的 4 B header。
- `N_WW_LOG_BOOT_RECORD_SIZE`：完整 boot record 大小，当前为 16 B。
- `LOG_EXT_FLUSH_MARKER_HDR`：flush marker 的 4 B encoded header。
- `LOG_EXT_FLUSH_MARKER_SIZE`：完整 marker 大小，当前为 8 B。

这些记录的 `pcnt` 仍然是合法值，所以 RAM walker、外存 cold-boot scan 和普通 entry parser 无需特殊修改就能越过它们；只有 decoder 在展示时解释其含义。

控制记录使用 ERR level，保证任何合法外存阈值下都能进入外存。它们不是业务错误，不会通过普通 `N_LOG_ERR` 调用生成。

### 3.3 `map_id` 的计算

`map_id` 是 SHA-256 结果的前 32 bit，输入只包含会改变解码含义的内容：

- encoding tag；
- 每个 `file_id -> path`；
- 每个 `(file_id, line, level, fmt)`。

以下内容故意不参与 hash：

- map 生成时间；
- JSON 格式和 key 顺序；
- module 当前是否静态使能；
- 派生出来的 short name；
- map 文件中已经保存的 `meta.map_id`。

这样，无关元数据变化不会产生新 ID，而任何可能导致日志被解释成另一句话的变化都会产生新 ID。`0x00000000` 和 `0xFFFFFFFF` 分别容易被解释为未初始化和擦除态，因此生成器不会使用这两个值。

同一算法在 `gen_log_map.py`、离线 decoder 和 JTAG decoder 中各有一份，序列化规则属于兼容协议，不能单独修改其中一份。

### 3.4 boot record 的写入和版本切换

`n_ww_log_init()` 在 RAM 初始化完成后调用 `n_ww_log_write_boot_record()`。它直接调用 `ww_log_backend_emit()`，绕过业务 module mask 和 runtime threshold，因此 boot record 会同时进入已使能的 UART、RAM，并通过正常 flush 路径进入外存。

外存写入还额外保证一个不变量：空归档第一次收到有效日志时，`log_ram_flush()` 会在该批日志前补一个 boot record。这样即使外存刚被 clear、RAM 随后又被清空，归档中也不会出现“有业务日志但前面没有 map 身份”的状态。

ERASE 策略触发整分区擦除时，新归档的第一批同样会带新的 boot record。

decoder 维护当前 map 状态：

- 遇到已归档的 `map_id`：切换到对应 map，后续日志可信。
- 遇到未知 `map_id`：切回命令行指定的默认 map，但给后续日志加 `?`，并输出可能解码错误的 warning。
- boot record 之前的旧日志：只能猜测使用默认 map，同样加 `?`。
- 未找到 map 时不会继续沿用上一个 boot 的可信 map，避免把新版本日志错误归到旧版本。

`BUILD_VERSION` 和 `BUILD_GIT_ID` 来自目标项目的 `version.h`；缺失时 FW 会写 0，功能仍可运行，但现场定位能力会下降。

### 3.5 file ID 稳定、复用和同名文件

生成新 map 时会先读取旧 map：

- 路径仍存在：保留原 offset。
- 新路径：取 module 内最低可用 offset。
- 已删除路径：不再写入新 map，其 offset 可以被后续新文件复用。

允许复用的前提是 boot record/map archive 已经建立。只永久保留 file ID 并不能解决旧日志问题，因为同一个文件中的日志行号会随代码编辑变化；真正的解码身份是整个 map，而不是单独的 file ID。

多个目录中的 `main.c` 路径不同，因此 file ID 一直不会重复。当前增加的是显示名消歧：生成器计算最短唯一尾路径，并同时用于 decoder 的文件名和 string 模式的 `__NOTDIR_FILE__`。

### 3.6 RAM ring 的可靠性变化

RAM 保留区仍以 `WLOG` header 开头，当前增强包括：

- `n_ww_log_init()` 默认采用 warm-preserve 初始化；header magic/checksum 有效时不清空历史数据。
- 除 header checksum 外，还会按每条 entry 的 `pcnt` 遍历数据区，检查 entry 链能否准确落到 `write_index`。
- 数据链异常时设置 `LOG_FLAG_CORRUPTED`，便于 host 提示现场 dump 可能不完整。
- 空间不足时按完整 entry 淘汰最旧日志，保证 `read_index` 始终落在 entry 边界。
- ERR entry 设置 `LOG_FLAG_ERROR`。
- FREEZE 外存装满时设置 `LOG_FLAG_EXT_FULL`，即使外存运行时 context 在冷启动后丢失，RAM dump 仍能说明外存为何停止增长。
- 日志在 `n_ww_log_init()` 之前过早输出时，RAM backend 会安全返回错误而不是解引用空 context。

### 3.7 外存追加式容器

当前 LOG 分区布局：

```text
log_offset
    +0   [XLOG partition header, 8 B]
    +8   [entry 0]
         [entry 1]
         [... boot / flush control records are also entries ...]
write_off
         [0xFF erased tail]
log_offset + log_size
```

初始化过程：

1. 根据系统信息选择 flash 或 EEPROM device。
2. 读取并校验 partition table。
3. 查找 `PART_ENTRY_TYPE_LOG`，保存 `part_offset` 和 `part_size`。
4. 检查分区至少能容纳 `XLOG` header 和一个最大 staging batch。
5. 如果 `XLOG` header 有效，从 `+8` 开始按 `4 + pcnt*4` 扫描到第一个 `0xFFFFFFFF`，恢复 `write_off`。
6. 只有首次使用、擦除态或 header 无效时才擦除 LOG 分区并写入新的 `XLOG` header。

因此外存容量判断始终使用 partition table 中的实际 `log_size`，不会继续搬运到分区以外。

每次 `log_ram_flush()` 最多扫描 `LOG_EXT_FLUSH_STAGE_SIZE` 的 RAM 数据，只复制完整 entry。`write_off` 和 RAM consume 在 mutex 下预留/提交；较慢的设备写操作在释放 mutex 后执行。当前实现使用静态 staging buffer，因此 flush 入口要求只有一个调用者，不能并发调用。

### 3.8 flush 触发和 marker

flush task 有两个唤醒来源：

- RAM `pending_len` 达到 `LOG_RAM_FLUSH_THRESHOLD` 时由 writer 发 semaphore。
- `CONFIG_N_LOG_FLUSH_TIMEOUT_MS` 到期后定时检查，确保少量日志最终也会搬运。

一次唤醒后会循环 drain，直到 RAM pending 为空、发生错误，或者 FREEZE 分区已满，而不是每次只搬一个 staging batch。

使能 `CONFIG_N_LOG_EXT_FLUSH_MARKER` 后，每次 task 唤醒会 arm 一次 marker；本轮 drain 的第一个真正包含持久化日志的 batch 前写入 `[marker header][tick]`。如果该 batch 的所有业务 entry 都被 ext threshold 过滤，则不会单独写一个空 marker。

### 3.9 外存满策略

`CONFIG_N_LOG_EXT_FULL=1`，FREEZE：

- 保留最早日志。
- 最后一批只写入还能放下的完整 entry 前缀，避免浪费接近一个 staging buffer 的尾部空间。
- 设置外存 full context 和 RAM `LOG_FLAG_EXT_FULL`。
- 后续 writer 不再反复唤醒只会失败的 flush task；RAM ring 和 UART 继续工作。

`CONFIG_N_LOG_EXT_FULL=2`，ERASE：

- 保留最新日志。
- 当前 batch 放不下时擦除整个 LOG 分区，重写 `XLOG` header。
- 在新归档的第一批前补 boot record，然后写入当前 batch。
- 对外报告为“永不冻结”，后续继续循环使用分区。

### 3.10 增量构建为什么需要多个派生文件

日志格式或行号变化会改变 map 和 map ID，但通常不会改变源文件的 file ID。如果所有对象都直接依赖每次重建的 `file_ids.mk`，一次普通日志编辑会导致整个项目重编。

当前做法是：

- `file_ids.mk` 供 Makefile 读取每个源文件的编译参数。
- `auto_file_ids.h` 使用 write-if-changed；只有文件新增、删除或 ID 变化才更新。
- `log_map_id.h` 使用 write-if-changed；只有 map ID 变化才更新。
- 普通对象依赖 `auto_file_ids.h`。
- 只有直接使用 `N_WW_LOG_MAP_ID` 的 `n_ww_log_control.c` 等对象依赖 `log_map_id.h`。

结果是普通 `.c` 编辑通常只重编自身；新增/删除源文件才保守地重编所有需要新 ID header 的对象；map ID 变化只额外重编少数消费者。

### 3.11 host 侧 EEPROM/LOG geometry

日志读取不再把“EEPROM 总容量”和“LOG 分区容量”混成同一个固定值：

- live JTAG reader 先读取设备起始区域中的 partition table，再只读取 LOG entry 指定的 offset/size。
- 离线 decoder 对整片 bin 执行相同的 partition table 解析，然后把解码范围裁到 LOG 分区。
- `eeprom_tool.py scan/info` 会扫描 `0x50..0x57`，按每个 ACK 地址代表一个 64 KiB bank 推算实际器件容量，并与底层 driver geometry 比较；不一致时报告 warning。
- EEPROM base address 未指定时，工具选择最低的 ACK 地址，并由驱动按访问地址计算 P1/P0 bank 位。

需要注意，当前容量探测是诊断和校验能力，不会在运行时自动改写 `eeprom_driver` 的编译期容量常量；搬入真实项目后仍应让 driver geometry 与器件规格一致。对日志读取而言，只要 partition table 可读，实际读取长度以 LOG partition size 为准。

## 4. 兼容性和升级注意事项

### 4.1 基线旧日志不能只靠新 map 解码

`484541271a2b` 使用 `file12_line14_pcnt6`，当前版本使用 `file12_line14_lvl2_pcnt4`。两者对 header 低 6 bit 的解释不同，当前 decoder 没有自动兼容旧编码。

此外，基线外存格式是 `LOGH` block + `FLOG` footer，当前格式是 `XLOG` append stream。升级后的 FW 不会把旧 `FLOG` 识别为有效 `XLOG`；初始化时可能把 LOG 分区视为旧/无效格式并重新擦除。

因此从基线升级时应先完成以下工作：

1. 使用旧 decoder 和旧 map 导出仍需保留的 RAM/外存日志。
2. 保存基线 decoder、map 和原始 dump，作为 legacy 解码工具链。
3. 接受升级后的 LOG 分区重新初始化，或另行实现一次性的旧格式迁移工具。
4. 从首次使用当前新编码格式的发布开始，严格执行 map archive 流程。

`BOOT_RECORD + map_id` 解决的是当前新 wire format 内部的跨 FW/map 版本问题，不是旧 wire format 到新 wire format 的自动迁移协议。

### 4.2 当前实现仍需注意的边界

- boot record 为了通过所有合法 ext threshold，编码 level 使用 ERR。它通过普通 RAM backend 写入时，当前 `log_ram_write()` 会同时设置 `LOG_FLAG_ERROR`。因此该 flag 目前表示“RAM 中出现过 ERR-level entry”，其中包含系统 boot record，并不能严格等同于“本次启动发生过业务错误”。如果现场逻辑需要后一个语义，应另行区分 control record 后再调整实现。
- 外存 flush 会先在 mutex 下预留 `write_off` 并 consume RAM，再在 mutex 外执行较慢的 device I/O。这样缩短了日志 writer 的阻塞时间，但 device write 失败时并没有事务回滚：该批 RAM pending 已被消费，`write_off` 也已推进。需要强掉电/写失败恢复能力时，应增加提交标记、CRC/双阶段提交，或调整 consume 时机。
- `log_ram_flush()` 使用一个静态 staging buffer，设计上只允许 flush task 单调用者。调试命令直接调用 flush 或 clear 外存时，也应在系统层避免与 flush task 并发。

### 4.3 partition table 常量必须与真实 FW 对齐

固件侧直接使用项目自己的 `PART_TABLE_T/PART_ENTRY_T`。离线 decoder 和当前仿真中的 partition table magic、LOG type ID、header/entry layout 是根据本地替身定义实现的，并在代码中标记了需要与真实 FW 再确认。

搬入内网时至少核对：

- partition table magic；
- `PART_ENTRY_TYPE_LOG` 数值；
- header/entry 字段布局和大小端；
- table 的搜索范围或固定位置；
- EEPROM 跨 I2C 地址 bank 的寻址方式和实际容量。

### 4.4 map 是发布产物，不只是临时构建文件

允许复用已删除文件的 ID 后，历史 map 更不能丢失。建议把以下内容作为一个发布集合保存：

```text
firmware.bin
version.h 或版本元数据
ww_log_map_<map_id>.json
对应版本的 decoder（至少在 wire format 变更时保存）
```

## 5. 文件职责

### 5.1 FW 功能文件

- `include/log/n_ww_log_def.h`：数值配置、编码布局、控制记录命名空间。
- `include/log/n_ww_log_macro.h`：string/encode/disable 宏展开和编译期阈值。
- `include/log/n_ww_log_api.h`：初始化、runtime level 和 module mask API、boot record API。
- `include/log/n_ww_log_output.h`、`log/n_ww_log_output.c`：两种输出模式和 backend fan-out。
- `include/log/n_ww_log_storage.h`、`log/n_ww_log_ram.c`：RAM ring、校验、完整 entry 淘汰与 ext filter packer。
- `log/n_ww_log_storage.c`：partition table、`XLOG`、恢复扫描、外存满策略和追加写入。
- `include/log/n_ww_log_task.h`、`log/n_ww_log_task.c`：mutex、semaphore、周期/阈值 flush。
- `log/n_ww_log_control.c`：运行时控制和 boot record 生成。

### 5.2 构建与 host 工具

- `scripts/log/gen_log_map.py`：扫描、file ID、short name、map ID、归档及派生产物。
- `scripts/log/log_config.json`：module 和扫描目录。
- `scripts/log/log_decoder.py`：离线 HEX/RAM/XLOG/整片 bin 解码和跨版本 map 切换。
- `host_driver/log_operation.py`、`host_driver/log_tool.py`：通过 JTAG 读取 RAM/flash/EEPROM 并复用同样的解码规则。
- `host_driver/eeprom_operation.py`、`host_driver/eeprom_tool.py`：EEPROM 容量探测、读写、烧录和清理支持。
- `Kconfig.fw`：供内网 FW 合并的 Kconfig 参考片段。

### 5.3 仅本地仿真

以下文件用于 PC 验证，不应当作 FW 功能代码搬入：

- `sim/*`
- `examples/main.c`
- 本仓库 `Makefile`
- `src/test/test_log.c`（可临时搬入目标板做验证，但不必进入产品源文件列表）

## 6. 当前验证覆盖

本地仿真目前覆盖并通过以下主要场景：

- `.conf -> autoconf.h` 数值和 bool 生成；
- compile/runtime/ext 三级阈值；
- UART、RAM、EXT_MEM backend 组合；
- RAM wrap、完整 entry 淘汰、header/data 校验、warm restart；
- 外存 partition table、`XLOG` 初始化和 cold-boot write offset 恢复；
- boot record 的 map/version/git 信息；
- FREEZE 尾部填充与 full flag；
- flush marker；
- map ID 归档和跨版本 decoder 切换；
- 普通增量构建和无变化重新构建。

当前默认 encode + UART/RAM/EXT_MEM + FREEZE 配置的仿真自测结果为 `59 passed, 0 failed`。
