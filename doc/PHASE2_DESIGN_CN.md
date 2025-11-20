# Phase 2 功能设计文档

## 版本信息

- **版本：** 2.0
- **日期：** 2025-11-20
- **分支：** claude/phase2-timestamp-mapping-011tV3zmvEoiEYMcUAX2qReE
- **状态：** 设计中

---

## Phase 2 目标

在 Phase 1 的基础上，添加两个重要功能：

1. **时间戳支持** - 为每条日志添加时间信息
2. **格式字符串映射表** - 解码时恢复原始消息

这两个功能都不会影响代码体积优化目标。

---

## 功能1：时间戳支持

### 设计目标

- 为每条日志添加时间戳
- 可配置开关（编译时）
- 代码体积增加 < 500字节
- RAM使用增加：每条日志 +4字节

### 配置选项

```c
/* 在 ww_log_config.h 中添加 */

/**
 * @brief Enable timestamp support
 * When enabled, each log entry includes a timestamp
 * RAM overhead: +4 bytes per log entry
 * Code overhead: ~300 bytes
 */
#define CONFIG_WW_LOG_TIMESTAMP_EN      1

/**
 * @brief Timestamp resolution
 * 0: Milliseconds since boot (U32, 0-4294967295ms, ~49.7 days)
 * 1: Seconds since boot (U32, 0-4294967295s, ~136 years)
 * 2: Tick count (platform specific)
 */
#define CONFIG_WW_LOG_TIMESTAMP_TYPE    0  /* Milliseconds */

/**
 * @brief Timestamp overflow handling
 * 0: Wrap around (default)
 * 1: Saturate at max value
 */
#define CONFIG_WW_LOG_TIMESTAMP_OVERFLOW  0
```

### 时间戳格式

#### ENCODE 模式的日志条目结构

**无时间戳（Phase 1）：**
```
[HEADER] [PARAM1] [PARAM2] ...
```

**有时间戳（Phase 2）：**
```
[HEADER_WITH_TIMESTAMP_FLAG] [TIMESTAMP] [PARAM1] [PARAM2] ...
```

#### Header编码格式扩展

**原始格式（32位）：**
```
 31                    20 19                8 7        2 1      0
┌──────────────────────┬────────────────────┬──────────┬────────┐
│   File ID (12 bits)  │  Line No (12 bits) │next_len  │ Level  │
│      0-4095          │     0-4095         │ (6 bits) │(2 bits)│
└──────────────────────┴────────────────────┴──────────┴────────┘
```

**扩展方案1（推荐）：使用next_len的最高位作为时间戳标志**
```
 31                    20 19                8 7  6     2 1      0
┌──────────────────────┬────────────────────┬──┬───────┬────────┐
│   File ID (12 bits)  │  Line No (12 bits) │TS│params │ Level  │
│      0-4095          │     0-4095         │1b│5 bits │(2 bits)│
└──────────────────────┴────────────────────┴──┴───────┴────────┘

TS: Timestamp flag (1 bit)
  0 = 无时间戳
  1 = 有时间戳（后续紧跟一个U32时间戳）

params: 参数个数 (5 bits, 0-31)
```

### 时间戳获取API

```c
/**
 * @brief Get current timestamp
 * @return Timestamp value (milliseconds or ticks)
 *
 * Platform specific implementation:
 * - Linux: clock_gettime(CLOCK_MONOTONIC)
 * - Windows: GetTickCount64() or timeGetTime()
 * - MSYS2: clock_gettime() if available, else GetTickCount()
 * - Embedded: Platform-specific timer (HAL_GetTick, xTaskGetTickCount, etc.)
 */
U32 ww_log_get_timestamp(void);
```

### 跨平台实现

```c
/* In ww_log_timestamp.c */

#ifdef CONFIG_WW_LOG_TIMESTAMP_EN

#if defined(__linux__) || defined(__MSYS__)
    /* Linux / MSYS2 */
    #include <time.h>

    U32 ww_log_get_timestamp(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (U32)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }

#elif defined(_WIN32) || defined(_WIN64)
    /* Windows */
    #include <windows.h>

    U32 ww_log_get_timestamp(void) {
        return (U32)GetTickCount();  /* Milliseconds since boot */
    }

#else
    /* Embedded or other platforms */
    /* User must provide implementation */
    __attribute__((weak)) U32 ww_log_get_timestamp(void) {
        /* Default: return 0 or tick counter */
        return 0;
    }
#endif

#endif /* CONFIG_WW_LOG_TIMESTAMP_EN */
```

### STRING 模式输出格式

**无时间戳：**
```
[INF][DEMO] demo_init.c:17 - Demo module initializing...
```

**有时间戳：**
```
[12345ms][INF][DEMO] demo_init.c:17 - Demo module initializing...
```

### ENCODE 模式输出格式

**无时间戳：**
```
0x00101102
```

**有时间戳：**
```
0x00101182   # Header with timestamp flag
0x00003039   # Timestamp: 12345ms
```

### RAM使用分析

**缓冲区大小计算：**

| 配置 | 每条日志 | 1024条日志 | 说明 |
|------|----------|-----------|------|
| 无时间戳 | 4 + N×4 字节 | 4KB + 参数 | Phase 1 |
| 有时间戳 | 4 + 4 + N×4 字节 | 8KB + 参数 | Phase 2 |

**示例：**
- 日志条目：文件ID=1, 行号=17, 级别=INF, 2个参数, 有时间戳
- 占用空间：4(header) + 4(timestamp) + 4×2(params) = **16字节**

### 代码实现

#### 1. 修改encode函数

```c
void ww_log_encode_2(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                     U16 file_id, U16 line, U32 param1, U32 param2)
{
    U8 param_count = 2;

#ifdef CONFIG_WW_LOG_TIMESTAMP_EN
    /* 设置时间戳标志位 */
    U8 ts_flag = 1;
    U32 timestamp = ww_log_get_timestamp();
#else
    U8 ts_flag = 0;
#endif

    /* 编码header (bit 7 = timestamp flag) */
    U32 header = (file_id << 20) | (line << 8) | (ts_flag << 7) | (param_count << 2) | level;

    /* 写入buffer */
    ww_log_ram_write(header);

#ifdef CONFIG_WW_LOG_TIMESTAMP_EN
    ww_log_ram_write(timestamp);
#endif

    ww_log_ram_write(param1);
    ww_log_ram_write(param2);
}
```

#### 2. 修改decoder支持时间戳

```python
def decode_header(header):
    file_id = (header >> 20) & 0xFFF
    line = (header >> 8) & 0xFFF
    has_timestamp = (header >> 7) & 0x1     # 新增：时间戳标志
    param_count = (header >> 2) & 0x1F      # 修改：5位参数计数
    level = header & 0x03

    return {
        'file_id': file_id,
        'line': line,
        'has_timestamp': has_timestamp,
        'param_count': param_count,
        'level': level
    }

def parse_log_entry(data_stream):
    header = read_u32(data_stream)
    info = decode_header(header)

    timestamp = None
    if info['has_timestamp']:
        timestamp = read_u32(data_stream)

    params = []
    for i in range(info['param_count']):
        params.append(read_u32(data_stream))

    return {
        'timestamp': timestamp,
        'file_id': info['file_id'],
        'line': info['line'],
        'level': info['level'],
        'params': params
    }
```

---

## 功能2：格式字符串映射表

### 设计目标

- 解码时恢复原始格式字符串
- 映射表不包含在嵌入式二进制中
- 支持参数格式化显示
- 完全不影响代码体积

### 实现方案

#### 1. 编译时提取格式字符串

**工具：** `tools/extract_log_strings.py`

**功能：**
- 扫描所有.c源文件
- 查找所有 `TEST_LOG_XXX_MSG()` 调用
- 提取文件ID、行号、格式字符串
- 生成JSON映射表

**示例输出：** `log_strings.json`
```json
{
  "format_version": "1.0",
  "generation_time": "2025-11-20 10:30:00",
  "entries": {
    "1:17": {
      "file": "demo_init.c",
      "function": "demo_init",
      "level": "INF",
      "format": "Demo module initializing..."
    },
    "1:25": {
      "file": "demo_init.c",
      "function": "demo_init",
      "level": "INF",
      "format": "Hardware check passed, code=%d"
    },
    "1:30": {
      "file": "demo_init.c",
      "function": "demo_init",
      "level": "WRN",
      "format": "Demo init completed with warnings, total_checks=%d, failed=%d"
    }
  }
}
```

#### 2. 提取脚本实现

```python
#!/usr/bin/env python3
"""
extract_log_strings.py - Extract log format strings from source code

Usage:
    python3 extract_log_strings.py src/ -o log_strings.json
"""

import re
import json
import os
import sys
from pathlib import Path

def extract_file_id(c_file):
    """Extract CURRENT_FILE_ID from C source file"""
    with open(c_file, 'r') as f:
        content = f.read()
        match = re.search(r'#define\s+CURRENT_FILE_ID\s+FILE_ID_(\w+)', content)
        if match:
            # Look up in log_file_id.h
            return get_file_id_value(match.group(1))
    return None

def extract_log_calls(c_file):
    """Extract all TEST_LOG_XXX_MSG calls from C file"""
    with open(c_file, 'r') as f:
        content = f.read()

    pattern = r'TEST_LOG_(ERR|WRN|INF|DBG)_MSG\s*\(\s*"([^"]+)"'
    matches = re.finditer(pattern, content)

    logs = []
    for match in matches:
        level = match.group(1)
        format_str = match.group(2)
        line_no = content[:match.start()].count('\n') + 1

        logs.append({
            'line': line_no,
            'level': level,
            'format': format_str
        })

    return logs

def generate_mapping(src_dir):
    """Generate complete mapping from source directory"""
    mapping = {
        'format_version': '1.0',
        'generation_time': datetime.now().isoformat(),
        'entries': {}
    }

    for c_file in Path(src_dir).rglob('*.c'):
        file_id = extract_file_id(c_file)
        if file_id is None:
            continue

        logs = extract_log_calls(c_file)

        for log in logs:
            key = f"{file_id}:{log['line']}"
            mapping['entries'][key] = {
                'file': c_file.name,
                'level': log['level'],
                'format': log['format']
            }

    return mapping
```

#### 3. 增强的解码器

```python
def decode_with_mapping(encoded_data, mapping_file):
    """Decode logs with format string mapping"""

    # Load mapping
    with open(mapping_file, 'r') as f:
        mapping = json.load(f)

    # Decode entry
    entry = parse_log_entry(encoded_data)

    # Look up format string
    key = f"{entry['file_id']}:{entry['line']}"
    if key in mapping['entries']:
        fmt_info = mapping['entries'][key]
        format_str = fmt_info['format']

        # Format with parameters
        try:
            formatted = format_str % tuple(entry['params'])
        except:
            formatted = f"{format_str} (params: {entry['params']})"

        # Generate output
        timestamp_str = ""
        if entry['timestamp'] is not None:
            timestamp_str = f"[{entry['timestamp']}ms]"

        print(f"{timestamp_str}[{get_level_str(entry['level'])}][{get_module_str(entry['file_id'])}] {fmt_info['file']}:{entry['line']}")
        print(f"  原始消息: \"{format_str}\"")
        if entry['params']:
            print(f"  参数值: {entry['params']}")
            print(f"  格式化输出: \"{formatted}\"")
    else:
        # No mapping found
        print(f"[{get_level_str(entry['level'])}] {entry['file_id']}:{entry['line']} - (no format string)")
        print(f"  Params: {entry['params']}")
```

#### 4. 使用示例

```bash
# 1. 提取格式字符串映射
python3 tools/extract_log_strings.py src/ -o log_strings.json

# 2. 使用映射解码日志
./bin/log_test 2>&1 | grep "^0x" > encode_output.txt
python3 tools/log_decoder.py --mapping log_strings.json encode_output.txt
```

#### 5. 增强的解码输出

**无映射表：**
```
[INF][DEMO] 1:17 (demo_init.c)
[INF][DEMO] 1:25 (demo_init.c) - Params: [0x00000000 (0)]
[WRN][DEMO] 1:30 (demo_init.c) - Params: [0x00000005 (5), 0x00000001 (1)]
```

**有映射表：**
```
[12345ms][INF][DEMO] demo_init.c:17
  原始消息: "Demo module initializing..."

[12378ms][INF][DEMO] demo_init.c:25
  原始消息: "Hardware check passed, code=%d"
  参数值: [0]
  格式化输出: "Hardware check passed, code=0"

[12405ms][WRN][DEMO] demo_init.c:30
  原始消息: "Demo init completed with warnings, total_checks=%d, failed=%d"
  参数值: [5, 1]
  格式化输出: "Demo init completed with warnings, total_checks=5, failed=1"
```

### Makefile集成

```makefile
# 生成格式字符串映射表
.PHONY: gen-mapping
gen-mapping:
	@echo "Generating log format string mapping..."
	@python3 tools/extract_log_strings.py src/ -o log_strings.json
	@echo "Mapping saved to: log_strings.json"

# 带映射的解码
.PHONY: decode-with-mapping
decode-with-mapping: gen-mapping
	@echo "Decoding with format string mapping..."
	@python3 tools/log_decoder.py --mapping log_strings.json encode_output.txt
```

---

## 代码体积影响分析

### 时间戳功能

| 组件 | 代码大小 | RAM使用 |
|------|----------|---------|
| 时间获取函数 | ~200字节 | 0 |
| encode函数增量 | ~100字节 | 0 |
| 配置和宏 | ~50字节 | 0 |
| **总计** | **~350字节** | 0 |

**RAM使用（运行时）：**
- 每条日志 +4字节（时间戳）
- 1024条日志：+4KB

### 格式字符串映射表

| 组件 | 代码大小 | RAM使用 |
|------|----------|---------|
| 提取工具（Python） | 0（不在嵌入式设备上） | 0 |
| 映射表（JSON） | 0（不在嵌入式设备上） | 0 |
| 解码器增强 | 0（不在嵌入式设备上） | 0 |
| **总计** | **0字节** | **0** |

**结论：** 格式字符串映射表完全不影响嵌入式代码体积！

---

## 兼容性

### 向后兼容

**Phase 1的日志（无时间戳）：**
- Bit 7 = 0，解码器识别为无时间戳
- 正常解码，显示为无时间戳

**Phase 2的日志（有时间戳）：**
- Bit 7 = 1，解码器读取时间戳
- 显示时间戳信息

**混合日志：**
- 支持同一个日志文件中混合有无时间戳的条目
- 解码器自动识别

### 跨平台兼容性

| 平台 | 时间戳API | 精度 | 状态 |
|------|----------|------|------|
| Linux | `clock_gettime()` | 纳秒 | ✅ |
| MSYS2 | `clock_gettime()` | 纳秒 | ✅ |
| Windows | `GetTickCount()` | 毫秒 | ✅ |
| 嵌入式 | 用户提供 | 自定义 | ✅ (weak函数) |

---

## 测试计划

### 时间戳功能测试

1. **功能测试**
   - [ ] 时间戳正确获取
   - [ ] 时间戳正确编码
   - [ ] 时间戳正确解码
   - [ ] 时间戳显示格式正确

2. **跨平台测试**
   - [ ] Linux编译和运行
   - [ ] MSYS2编译和运行
   - [ ] Windows编译和运行

3. **性能测试**
   - [ ] 代码体积增加 < 500字节
   - [ ] 运行时性能无明显下降

### 格式字符串映射测试

1. **功能测试**
   - [ ] 正确提取所有日志调用
   - [ ] 正确提取文件ID和行号
   - [ ] 正确提取格式字符串
   - [ ] JSON格式正确

2. **解码测试**
   - [ ] 正确加载映射表
   - [ ] 正确查找格式字符串
   - [ ] 正确格式化参数
   - [ ] 无映射时正常降级

3. **集成测试**
   - [ ] Makefile目标正常工作
   - [ ] 端到端流程测试

---

## 文档清单

### 新增文档

1. `doc/PHASE2_DESIGN_CN.md` - 本文档
2. `doc/TIMESTAMP_USAGE_CN.md` - 时间戳使用指南
3. `doc/MAPPING_USAGE_CN.md` - 映射表使用指南
4. `README_PHASE2.md` - Phase 2 快速开始

### 更新文档

1. `README.md` - 添加 Phase 2 说明
2. `IMPLEMENTATION_SUMMARY.md` - 更新功能列表
3. `TEST_RESULTS.md` - 添加 Phase 2 测试结果

---

## 实现时间表

| 任务 | 预计时间 | 状态 |
|------|---------|------|
| 时间戳配置和API | 30分钟 | ⏳ 待开始 |
| 时间戳编码实现 | 1小时 | ⏳ 待开始 |
| 时间戳解码实现 | 30分钟 | ⏳ 待开始 |
| 提取工具实现 | 1小时 | ⏳ 待开始 |
| 解码器增强 | 1小时 | ⏳ 待开始 |
| 测试和调试 | 1小时 | ⏳ 待开始 |
| 文档编写 | 1小时 | ⏳ 待开始 |
| **总计** | **约6小时** | |

---

**文档作者：** Claude AI
**版本：** 2.0
**状态：** Phase 2 设计完成，准备实现
