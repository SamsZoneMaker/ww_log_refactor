# MCU SDK Log机制重构需求与架构设计

## 项目概述
重构MCU SDK上的日志（log）机制，从传统的字符串打印模式转向支持编码模式的轻量级日志系统。目标是在MCU资源受限的环境下，实现高效、灵活、可扩展的日志功能，同时支持调试和量产两种场景。系统需考虑代码大小（code size）、RAM大小（RAM size）和日志准确性（log accuracy）。

## 核心需求
### 1. 日志模式
- **str_mode**：传统字符串模式，直接打印到UART。包含func、line、string、单个或多个data。适用于前期调试，空间充足时使用。
- **ecode_mode**：编码模式，使用U32编码日志。编译时自动将func+line+string+data转换为file id+line+data，丢弃string以节省空间。适用于量产环境。

### 2. 编码格式设计
- 每个日志编码为U32（32位）。
- 位分配（可通过宏定义调整）：
  - file id：12bit（文件ID，可能进一步拆分为dir id + file id以支持目录结构）。
  - line：12bit（行号）。
  - next data len：8bit（后续数据长度，表示后面跟随多少个U32 data，最多255个）。
- data存储：所有data（8bit、16bit、24bit、32bit）统一存储为U32，以简化设计。
- 宏定义：所有位分配和最大值（如next data len max）使用宏定义，便于调整。

### 3. 文件和函数标识
- func id：使用enum定义函数ID。
- file id：12bit，可能拆分：
  - dir id：前几位（例如4bit，用于目录ID）。
  - file id：剩余位（例如8bit，用于文件ID）。
- 避免同名文件冲突，通过dir id区分。

### 4. 静态模式选择
- 通过编译时宏或配置静态选择str_mode或ecode_mode。

### 5. 动态过滤扩展
- 支持动态修改变量控制日志level（级别）和module（模块）。
- 过滤不需要的日志信息，提高效率。

### 6. 存储扩展
- 支持将日志保存到RAM、外存（如EEPROM、SPI NOR）。
- 预留机制以支持未来扩展。

### 7. 解析工具
- 设计脚本：解析ecode_mode日志，将编码转换为字符可视化日志。使用file id enum映射回文件名/函数名。丢弃的string信息无法恢复。
- GUI工具扩展：开发Windows GUI工具，动态解析ecode_mode日志，实时打印并添加timestamp（解决timestamp缺失问题）。

## 架构设计
### 系统组件
1. **日志宏和API**：
   - 定义日志宏（如LOG_INFO、LOG_ERROR），根据模式自动选择str_mode或ecode_mode。
   - API支持变参data输入。

2. **编码生成器**：
   - 编译时工具或宏，自动生成file id、line映射。
   - enum定义：为每个文件和函数分配ID。

3. **过滤机制**：
   - 全局变量：level_mask、module_mask。
   - 运行时动态修改，支持enable/disable特定level和module。

4. **存储层**：
   - RAM缓冲区。
   - 接口支持外存写入（EEPROM、SPI NOR）。

5. **解析工具**：
   - 脚本：Python/C++脚本，读取日志流，解码U32，映射回file/line/data。
   - GUI：基于Qt或WinForms，实时解析，添加timestamp。

### 目录结构规划（Windows调试环境）
```
d:/workspace/log/
├── include/                 # 公共头文件
│   ├── log_config.h         # 宏定义、模式选择
│   ├── log_types.h          # 类型定义、enum
│   └── log_api.h            # API声明
├── src/                     # 核心实现
│   ├── log_core.c           # 核心日志逻辑
│   ├── log_encode.c         # 编码模式实现
│   ├── log_str.c            # 字符串模式实现
│   └── log_filter.c         # 过滤机制
├── modules/                 # 示例模块
│   ├── driver/              # 驱动模块
│   │   ├── uart_driver.c
│   │   ├── spi_driver.c
│   │   └── i2c_driver.c
│   ├── app/                 # 应用模块
│   │   ├── main_app.c
│   │   └── task_manager.c
│   └── utils/               # 工具模块
│       ├── crc_utils.c
│       └── timer_utils.c
├── tools/                   # 工具脚本
│   ├── log_parser.py        # Python解析脚本
│   └── log_gui/             # GUI工具源码
│       ├── main.cpp
│       └── gui.ui
├── test/                    # 测试文件
│   ├── test_log.c
│   └── test_encode.c
├── docs/                    # 文档
│   ├── README.md
│   └── design.md
└── build/                   # 构建脚本（Makefile或CMake）
    ├── Makefile
    └── CMakeLists.txt
```

### 设计考虑
- **Code Size**：编码模式最小化字符串存储；宏定义优化位分配。
- **RAM Size**：缓冲区大小可配置；动态分配避免浪费。
- **Log Accuracy**：精确映射file id/line；data完整性校验（可选CRC）。
- **扩展性**：预留位字段用于未来功能（如priority、timestamp嵌入）。
- **兼容性**：str_mode与现有代码兼容；ecode_mode无缝切换。

## 实现步骤（AI编程提示词）
1. **初始化项目结构**：创建上述目录，编写基础头文件（log_config.h、log_types.h）。
2. **实现核心API**：定义日志宏，支持str_mode和ecode_mode。
3. **编码生成**：设计enum和宏，自动生成file id/line映射。
4. **过滤机制**：实现动态level/module控制。
5. **存储扩展**：添加RAM和外存接口。
6. **解析工具**：编写Python脚本解析日志。
7. **GUI工具**：开发Windows GUI，集成timestamp。
8. **测试与调试**：在各模块添加日志，验证编码和解析。

此设计全面覆盖需求，支持迭代开发。请按此Markdown逐步实现。