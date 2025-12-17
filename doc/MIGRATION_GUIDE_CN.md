# Phase 1 日志系统迁移指南

> **目标：** 将Phase 1日志系统集成到现有项目
> **适用项目：** 嵌入式C项目，使用Shell脚本或Makefile编译
> **预计时间：** 2-4小时（取决于项目规模）

---

## 📦 第一步：复制核心文件（7个文件）

### 1.1 头文件（复制3个文件到你的 `include/` 目录）

```bash
# 从测试项目复制到你的项目
cp ww_log_refactor/include/ww_log.h         your_project/include/
cp ww_log_refactor/include/ww_log_config.h  your_project/include/
cp ww_log_refactor/include/log_file_id.h    your_project/include/
```

**说明：**
- `ww_log.h` - 核心API，包含所有日志宏（`TEST_LOG_XXX_MSG`）
- `ww_log_config.h` - 配置文件（**需要修改**，见第二步）
- `log_file_id.h` - 文件ID枚举（**需要完全重写**，见第三步）

---

### 1.2 实现文件（复制3个文件到你的 `src/` 目录）

```bash
# 创建日志核心目录
mkdir -p your_project/src/log_core

# 复制实现文件
cp ww_log_refactor/src/core/ww_log_common.c  your_project/src/log_core/
cp ww_log_refactor/src/core/ww_log_str.c     your_project/src/log_core/
cp ww_log_refactor/src/core/ww_log_encode.c  your_project/src/log_core/
```

**说明：**
- `ww_log_common.c` - 初始化和公共逻辑
- `ww_log_str.c` - STRING模式实现（printf风格）
- `ww_log_encode.c` - ENCODE模式实现（二进制编码）

---

### 1.3 工具文件（可选，复制到 `tools/` 目录）

```bash
mkdir -p your_project/tools

# 必需：ENCODE模式日志解码器
cp ww_log_refactor/tools/log_decoder.py  your_project/tools/

# 可选：二进制大小对比工具
cp ww_log_refactor/tools/size_compare.py  your_project/tools/
```

---

## ⚙️ 第二步：修改配置文件

### 2.1 编辑 `ww_log_config.h`

打开 `your_project/include/ww_log_config.h`，修改以下部分：

#### **选择日志模式（三选一）**

```c
/* ========== Log Mode Selection (Choose ONE) ========== */

// #define CONFIG_WW_LOG_DISABLED       /* 无日志输出 */
#define CONFIG_WW_LOG_STR_MODE          /* 字符串模式（调试用） */
// #define CONFIG_WW_LOG_ENCODE_MODE    /* 编码模式（生产用） */
```

**建议：**
- 开发阶段：使用 `CONFIG_WW_LOG_STR_MODE`
- 生产部署：使用 `CONFIG_WW_LOG_ENCODE_MODE`

---

#### **定义你的项目模块**

```c
/* ========== Module ID Enumeration ========== */

typedef enum {
    WW_LOG_MOD_INIT = 0,        // 系统初始化模块
    WW_LOG_MOD_DRIVERS = 1,     // 驱动层
    WW_LOG_MOD_ALGORITHM = 2,   // 算法模块
    WW_LOG_MOD_APP = 3,         // 应用层
    WW_LOG_MOD_COMM = 4,        // 通信模块
    WW_LOG_MOD_MAX              // 必须保留
} WW_LOG_MODULE_E;
```

**根据你的项目实际模块修改！**

---

#### **配置模块静态开关**

```c
/* ========== Module Static Enable/Disable Switches ========== */

#define CONFIG_WW_LOG_MOD_INIT_EN       1
#define CONFIG_WW_LOG_MOD_DRIVERS_EN    1
#define CONFIG_WW_LOG_MOD_ALGORITHM_EN  1
#define CONFIG_WW_LOG_MOD_APP_EN        1
#define CONFIG_WW_LOG_MOD_COMM_EN       1
```

**对应上面的模块枚举！**

---

#### **调整RAM缓冲区大小（根据嵌入式系统RAM大小）**

```c
/* ========== RAM Buffer Configuration ========== */

#define CONFIG_WW_LOG_RAM_ENTRY_NUM  256  // 256条×4字节=1KB
```

**建议值：**
- RAM充足（>64KB）：1024条（4KB）
- RAM紧张（16-64KB）：256条（1KB）
- RAM极紧张（<16KB）：64条（256字节）

---

#### **配置输出目标**

```c
/* ========== Output Targets ========== */

#define CONFIG_WW_LOG_OUTPUT_UART  1  // 实时UART输出
#define CONFIG_WW_LOG_OUTPUT_RAM   1  // RAM缓冲存储
```

**推荐组合：**
- 调试阶段：UART=1, RAM=0（只看实时输出）
- 生产环境：UART=0, RAM=1（只存储，节省性能）
- 综合方案：UART=1, RAM=1（实时监控+历史记录）

---

## 📝 第三步：重写 `log_file_id.h`

### 3.1 统计你的项目文件

```bash
# 统计每个模块的.c文件数量
find src/init -name "*.c" | wc -l       # 假设10个
find src/drivers -name "*.c" | wc -l    # 假设50个
find src/algorithm -name "*.c" | wc -l  # 假设80个
find src/app -name "*.c" | wc -l        # 假设120个
find src/comm -name "*.c" | wc -l       # 假设30个
```

---

### 3.2 分配ID范围

根据文件数量预留**30%扩展空间**：

| 模块 | 文件数 | 预留30% | ID范围 |
|------|--------|---------|--------|
| INIT | 10个 | 13个 | 1-20 |
| DRIVERS | 50个 | 65个 | 21-100 |
| ALGORITHM | 80个 | 104个 | 101-220 |
| APP | 120个 | 156个 | 221-400 |
| COMM | 30个 | 39个 | 401-450 |

---

### 3.3 编写 `log_file_id.h`

```c
/**
 * @file log_file_id.h
 * @brief File ID enumeration for your project
 */

#ifndef LOG_FILE_ID_H
#define LOG_FILE_ID_H

typedef enum {
    /* INIT Module (1-20) */
    FILE_ID_SYSTEM_INIT = 1,        /* src/init/system_init.c */
    FILE_ID_CLOCK_INIT = 2,         /* src/init/clock_init.c */
    FILE_ID_MEMORY_INIT = 3,        /* src/init/memory_init.c */
    // ... 其余7个init文件
    /* 11-20: Reserved */

    /* DRIVERS Module (21-100) */
    FILE_ID_UART_DRIVER = 21,       /* src/drivers/uart.c */
    FILE_ID_SPI_DRIVER = 22,        /* src/drivers/spi.c */
    FILE_ID_I2C_DRIVER = 23,        /* src/drivers/i2c.c */
    // ... 其余47个驱动文件
    /* 71-100: Reserved */

    /* ALGORITHM Module (101-220) */
    FILE_ID_FILTER_ALGO = 101,      /* src/algorithm/filter.c */
    FILE_ID_FFT_ALGO = 102,         /* src/algorithm/fft.c */
    // ... 其余78个算法文件
    /* 181-220: Reserved */

    /* APP Module (221-400) */
    FILE_ID_APP_MAIN = 221,         /* src/app/main.c */
    FILE_ID_APP_CONFIG = 222,       /* src/app/config.c */
    // ... 其余118个应用文件
    /* 341-400: Reserved */

    /* COMM Module (401-450) */
    FILE_ID_UART_PROTOCOL = 401,    /* src/comm/uart_protocol.c */
    FILE_ID_CAN_PROTOCOL = 402,     /* src/comm/can_protocol.c */
    // ... 其余28个通信文件
    /* 431-450: Reserved */

} LOG_FILE_ID_E;

#endif /* LOG_FILE_ID_H */
```

**重要规则：**
- ✅ 按字母顺序排列，方便查找
- ✅ 每个ID对应一个.c文件
- ✅ 注释中写明文件路径
- ✅ 预留扩展空间

---

## 🔧 第四步：在源文件中使用日志

### 4.1 在每个需要日志的 .c 文件开头添加：

```c
// src/drivers/uart.c

#include "ww_log.h"          // 日志系统API
#include "log_file_id.h"     // 文件ID枚举

// 定义当前文件的ID和模块ID（必须！）
#define CURRENT_FILE_ID   FILE_ID_UART_DRIVER
#define CURRENT_MODULE_ID WW_LOG_MOD_DRIVERS

// 你的代码...
void uart_init(void)
{
    TEST_LOG_INF_MSG("UART initializing...");

    int baud_rate = 115200;
    TEST_LOG_DBG_MSG("Baud rate: %d", baud_rate);

    if (init_failed) {
        TEST_LOG_ERR_MSG("UART init failed!");
        return -1;
    }

    TEST_LOG_INF_MSG("UART init success");
}
```

---

### 4.2 日志宏说明

| 宏 | 级别 | 用途 |
|---|------|------|
| `TEST_LOG_ERR_MSG(fmt, ...)` | ERROR | 严重错误（系统故障、数据损坏） |
| `TEST_LOG_WRN_MSG(fmt, ...)` | WARNING | 警告（潜在问题、资源不足） |
| `TEST_LOG_INF_MSG(fmt, ...)` | INFO | 重要信息（状态变化、关键事件） |
| `TEST_LOG_DBG_MSG(fmt, ...)` | DEBUG | 调试信息（详细执行流程） |

**支持参数：**
- 0个参数：`TEST_LOG_INF_MSG("System started")`
- 1个参数：`TEST_LOG_DBG_MSG("Value: %d", value)`
- 2个参数：`TEST_LOG_ERR_MSG("Error %d at addr 0x%X", code, addr)`
- 3个参数：`TEST_LOG_WRN_MSG("A=%d B=%d C=%d", a, b, c)`

---

## 🛠️ 第五步：修改编译脚本

### 5.1 Shell脚本编译（你的现有方式）

在你的编译脚本中添加日志模块编译：

```bash
#!/bin/bash
# your_build_script.sh

# 编译日志系统核心
gcc -c src/log_core/ww_log_common.c -Iinclude -O2 -Wall -o build/ww_log_common.o
gcc -c src/log_core/ww_log_str.c    -Iinclude -O2 -Wall -o build/ww_log_str.o
gcc -c src/log_core/ww_log_encode.c -Iinclude -O2 -Wall -o build/ww_log_encode.o

# 编译你的源文件
for file in src/**/*.c; do
    obj_file="build/$(basename ${file%.c}).o"
    gcc -c $file -Iinclude -O2 -Wall -o $obj_file
done

# 链接（包含日志模块的.o文件）
gcc build/*.o -o bin/your_app
```

---

### 5.2 Makefile编译（可选，性能更好）

如果你愿意迁移到Makefile，可以参考测试项目的Makefile：

```bash
cp ww_log_refactor/Makefile your_project/
# 然后修改源文件路径
```

**Makefile优势：**
- ✅ 增量编译（只编译改动的文件）
- ✅ 并行编译（充分利用多核CPU）
- ✅ 自动依赖追踪（.h文件改动自动重编相关.c）

---

## 🚀 第六步：初始化日志系统

### 6.1 在 `main()` 函数中初始化

```c
#include "ww_log.h"

int main(void)
{
    // 初始化日志系统（必须！）
    ww_log_init();

    TEST_LOG_INF_MSG("System starting...");

    // 你的应用代码
    // ...

    return 0;
}
```

---

### 6.2 （可选）注册自定义UART输出

如果你的项目有自己的UART驱动：

```c
// 自定义UART输出函数
void my_uart_output(const char *msg)
{
    uart_puts(msg);  // 调用你的UART驱动
}

int main(void)
{
    ww_log_init();

    // 注册自定义输出钩子
    ww_log_str_hook_install(my_uart_output);

    // ...
}
```

**ENCODE模式类似：**
```c
void my_encode_uart_output(uint32_t encoded_log)
{
    uart_write_u32(encoded_log);
}

ww_log_encode_hook_install(my_encode_uart_output);
```

---

## ✅ 第七步：编译和测试

### 7.1 首次编译

```bash
# 执行你的编译脚本
./your_build_script.sh

# 检查是否有编译错误
```

**常见编译错误：**

| 错误 | 原因 | 解决方法 |
|------|------|----------|
| `undefined reference to ww_log_init` | 没有链接日志.o文件 | 检查编译脚本，确保链接了3个log_core的.o |
| `CURRENT_FILE_ID undeclared` | 源文件没定义宏 | 在.c文件开头添加 `#define CURRENT_FILE_ID ...` |
| `FILE_ID_XXX undeclared` | log_file_id.h中没定义 | 检查log_file_id.h枚举 |

---

### 7.2 运行测试

```bash
# 运行程序
./bin/your_app

# STRING模式输出示例：
# [INF][DRIVERS] uart.c:15 - UART initializing...
# [DBG][DRIVERS] uart.c:18 - Baud rate: 115200
# [INF][DRIVERS] uart.c:25 - UART init success
```

---

### 7.3 解码ENCODE模式日志

如果使用ENCODE模式：

```bash
# 运行程序，输出保存到文件
./bin/your_app > logs/output.bin

# 解码日志
python3 tools/log_decoder.py logs/output.bin

# 输出示例：
# [INF] FILE_ID_UART_DRIVER:15 - UART initializing
```

---

## 📊 第八步：验证代码大小减少

### 8.1 对比二进制大小

```bash
# STRING模式
make clean && make MODE=STRING
ls -lh bin/your_app  # 记录大小

# ENCODE模式
make clean && make MODE=ENCODE
ls -lh bin/your_app  # 记录大小

# 计算减少比例
python3 tools/size_compare.py
```

**预期效果：**
- 代码大小减少：**20-40%**（取决于日志数量）
- RAM使用减少：**50%以上**

---

## 🎯 快速检查清单

迁移完成后，检查以下项目：

### 必须完成的任务

- [ ] ✅ 复制了7个核心文件（3个头文件 + 3个实现 + 1个解码工具）
- [ ] ✅ 修改了 `ww_log_config.h` 定义项目模块
- [ ] ✅ 重写了 `log_file_id.h` 为所有文件分配ID
- [ ] ✅ 每个使用日志的.c文件定义了 `CURRENT_FILE_ID` 和 `CURRENT_MODULE_ID`
- [ ] ✅ 编译脚本中添加了日志模块编译
- [ ] ✅ `main()` 中调用了 `ww_log_init()`
- [ ] ✅ 编译通过，无错误
- [ ] ✅ 运行正常，日志输出正确

### 可选优化

- [ ] 💡 注册了自定义UART输出钩子
- [ ] 💡 启用了双输出（UART + RAM）
- [ ] 💡 验证了热重启后日志保留
- [ ] 💡 测试了ENCODE模式的代码大小减少
- [ ] 💡 迁移到Makefile实现增量编译

---

## ❓ 常见问题

### Q1: 需要为每个.c文件手动定义宏吗？

**A:** 是的，目前需要在每个使用日志的.c文件开头定义：
```c
#define CURRENT_FILE_ID   FILE_ID_YOUR_FILE
#define CURRENT_MODULE_ID WW_LOG_MOD_YOUR_MODULE
```

**原因：** C语言没有反射机制，无法自动知道"当前文件对应哪个ID"。

**未来优化：** 考虑使用自动生成工具（见 `claude/auto-file-id-generation` 分支）。

---

### Q2: log_file_id.h 太大了，有400个枚举怎么办？

**A:** 这是正常的，400个文件对应400个枚举定义。

**建议：**
- 按模块分组，加注释便于查找
- 使用文本编辑器的"查找"功能快速定位
- 未来可以用自动生成工具（Python脚本扫描源文件自动生成）

---

### Q3: 可以只在部分模块启用日志吗？

**A:** 可以！有两种方式：

**方式1：静态开关（编译时）**
```c
// ww_log_config.h
#define CONFIG_WW_LOG_MOD_DRIVERS_EN  1  // 启用
#define CONFIG_WW_LOG_MOD_ALGORITHM_EN 0  // 禁用
```

**方式2：动态开关（运行时）**
```c
// main函数中
g_ww_log_mod_enable[WW_LOG_MOD_ALGORITHM] = 0;  // 禁用算法模块日志
```

---

### Q4: ENCODE模式和STRING模式可以同时启用吗？

**A:** 不可以，三种模式互斥：
- `CONFIG_WW_LOG_DISABLED` - 无日志
- `CONFIG_WW_LOG_STR_MODE` - 字符串模式
- `CONFIG_WW_LOG_ENCODE_MODE` - 编码模式

只能在 `ww_log_config.h` 中选择一种。

---

### Q5: 如何调试日志系统本身？

**A:** 建议步骤：

1. **先用STRING模式验证逻辑**
   ```c
   #define CONFIG_WW_LOG_STR_MODE
   ```
   输出是人类可读的，容易调试。

2. **验证通过后切换到ENCODE模式**
   ```c
   #define CONFIG_WW_LOG_ENCODE_MODE
   ```

3. **使用解码工具检查**
   ```bash
   python3 tools/log_decoder.py output.bin
   ```

---

## 📚 参考文档

| 文档 | 路径 | 说明 |
|------|------|------|
| **Phase 1增强功能** | `doc/PHASE1_ENHANCEMENTS_CN.md` | 热重启、钩子、双输出说明 |
| **改进说明** | `doc/IMPROVEMENTS_CN.md` | Phase 1所有改进详情 |
| **格式字符串实验** | `doc/FORMAT_STRING_EXPERIMENT.md` | 代码大小减少验证 |
| **MSYS2兼容性** | `doc/MSYS2_COMPATIBILITY.md` | Windows环境支持 |

---

## 🎉 总结

### 核心步骤

1. **复制7个文件** → 3个头文件 + 3个实现 + 1个解码工具
2. **修改配置** → `ww_log_config.h` 定义你的模块
3. **分配文件ID** → `log_file_id.h` 为所有文件编号
4. **源文件定义宏** → 每个.c文件添加 `CURRENT_FILE_ID` 和 `CURRENT_MODULE_ID`
5. **集成编译** → 编译脚本中添加日志模块
6. **初始化** → `main()` 中调用 `ww_log_init()`
7. **测试验证** → 编译运行，检查日志输出

### 预期效果

- ✅ 代码大小减少 **20-40%**
- ✅ RAM使用减少 **50%以上**
- ✅ 日志容量提升 **12.5倍**（ENCODE模式）
- ✅ 完全向后兼容你的现有代码
- ✅ 三种模式可灵活切换

### 预计工作量

| 项目规模 | 文件数量 | 预计时间 |
|---------|---------|---------|
| **小型** | <50个文件 | 2小时 |
| **中型** | 50-200个文件 | 4小时 |
| **大型** | 200-500个文件 | 8小时 |

**耗时最多的部分：** 编写 `log_file_id.h` 和在每个源文件定义宏。

---

**祝迁移顺利！如有问题，参考测试项目代码：**
- 分支：`claude/phase1-enhancements-011tV3zmvEoiEYMcUAX2qReE`
- GitHub：https://github.com/SamsZoneMaker/ww_log_refactor

---

**文档结束**
