# 合入内网 FW 的建议顺序

本文以 Git 提交 `484541271a2b` 为基线，描述当前工作树需要搬运的内容。
`sim/` 只用于本地 PC 仿真，不属于 FW 功能实现。

## 0. 先归档旧 map

在生成新 map 前保存基线 FW 的 `ww_log_map.json`。新版日志会在 boot record 中写入
`map_id`；跨版本解码依赖 `maps/` 中仍能找到对应的历史 map。

## 1. 先搬生成与解码工具

按顺序更新：

1. `scripts/log/gen_log_map.py`
2. `scripts/log/log_config.json`
3. `scripts/log/log_decoder.py`

生成器负责递归扫描、稳定分配 file ID、处理同名 `main.c` 的路径短名、生成
`map_id`。解码器会依据外存 boot record 切换历史 map，并能从整片 bin 的分区表中
定位 LOG 分区。

随后把实际 FW 构建规则接到生成器：

- `file_ids.mk` 可每次扫描更新；
- `auto_file_ids.h` 和 `log_map_id.h` 必须使用 write-if-changed；
- 普通对象依赖 `auto_file_ids.h`；
- 直接使用 map ID 的 `n_ww_log_control.c` 依赖 `log_map_id.h`；
- 扫描根目录要递归，并把目录本身列为依赖，才能发现文件增删；
- 同名文件的编译参数必须使用生成器输出的 `SHORT_NAME_*`。

不要照搬本仓库 `Makefile`，它是 PC 仿真构建文件，只移植上述依赖关系。

## 2. 合入 Kconfig 与项目 `.conf`

把 `Kconfig.fw` 中的条目合并到内网 FW 自己的 Kconfig。单值策略使用数值枚举，
后端继续使用 bool：

```text
CONFIG_N_LOG=y
# mode: string=1, encode=2, disable=3
CONFIG_N_LOG_MODE=2

CONFIG_N_LOG_BACKEND_UART=y
CONFIG_N_LOG_BACKEND_RAM=y
CONFIG_N_LOG_BACKEND_EXT_MEM=y

# level: error=0, warning=1, info=2, debug=3
CONFIG_N_LOG_COMPILE_THRESHOLD=3
CONFIG_N_LOG_RUNTIME_THRESHOLD=3
CONFIG_N_LOG_EXT_LEVEL_THRESHOLD=1

# full policy: freeze=1, erase=2
CONFIG_N_LOG_EXT_FULL=1
CONFIG_N_LOG_EXT_FLUSH_MARKER=y

# 唯一保留在 .conf 中的 tuning
CONFIG_N_LOG_FLUSH_TIMEOUT_MS=10000
```

其余实现参数不再进入 `.conf`：

- `LOG_RAM_FLUSH_THRESHOLD`、`LOG_EXT_FLUSH_STAGE_SIZE` 在
  `include/log/n_ww_log_storage.h`；
- `LOG_WRITE_TIMEOUT_MS`、`LOG_FLUSH_TASK_STACK_SIZE`、
  `LOG_FLUSH_TASK_PRIORITY` 在 `include/log/n_ww_log_task.h`。

外存后端必须同时使能 RAM；RAM/外存只允许 encode 模式。Kconfig 和 C 头文件都会
校验这些约束。

## 3. 搬 FW 功能头文件

建议整体替换并按以下依赖顺序落入：

1. `include/log/n_ww_log_def.h`
2. `include/log/n_ww_log_api.h`
3. `include/log/n_ww_log_output.h`
4. `include/log/n_ww_log_macro.h`
5. `include/log/n_ww_log_task.h`
6. `include/log/n_ww_log_storage.h`
7. `include/log/n_ww_log.h`

这些文件包含新的数值配置判断、编码等级字段、boot/flush 控制记录、外存满标志和
头文件内实现参数。

## 4. 搬 FW 功能源文件

以下文件应作为一组合入：

1. `log/n_ww_log_ram.c`
2. `log/n_ww_log_task.c`
3. `log/n_ww_log_control.c`
4. `log/n_ww_log_storage.c`
5. `log/n_ww_log_output.c`

相对基线，最后一个文件由 `log/n_ww_logoutput.c` 重命名而来；要同步修改 FW 的源
文件列表。`control.c` 与 `storage.c` 通过 boot record 接口互相配合，不建议拆开
进入可发布版本。

目标项目需提供自己的 `version.h`，其中最好定义 `BUILD_VERSION` 和
`BUILD_GIT_ID`；缺失时当前代码会写 0，但会降低现场日志的版本辨识度。

## 5. 搬 host 侧读取工具

如果内网工具仍基于基线版本，整体同步：

- `host_driver/dora.py`
- `host_driver/eeprom_operation.py`
- `host_driver/eeprom_tool.py`
- `host_driver/log_operation.py`
- `host_driver/log_tool.py`

上硬件前核对真实项目的分区表 magic、entry type 和结构格式。host 工具应从整片
bin 的分区表取得存储总大小、LOG offset 与 LOG size，不能继续写死 EEPROM/LOG
大小。

## 6. 生成并归档发布产物

重新生成 `ww_log_map.json` 和 `log_map_id.h`，检查已有源文件的 file ID 未意外改变；
发布时把当前 map 复制到 `maps/ww_log_map_<map_id>.json` 并纳入版本控制。

## 7. 测试文件（可选）

`src/test/test_log.c` 可临时搬到目标板验证 RAM、外存、重启恢复、FREEZE/ERASE 和
跨版本 boot record。验证完成后可不进入产品源文件列表。

## 不要搬入 FW 的本地仿真文件

- `sim/*`
- `examples/main.c`
- 本仓库 `Makefile`
- `sim/log.conf`
- `sim/conf_to_autoconf.py`

`Kconfig.fw` 是集成参考片段，应把条目合入 FW 已有 Kconfig，而不是让产品同时维护
两套顶层 Kconfig。
