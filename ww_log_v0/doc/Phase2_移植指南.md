# LOG模块 Phase2 移植指南

## 文档说明

本文档明确标注哪些代码是**仿真专用**的，哪些是**需要移植到服务器项目**的。

---

## 1. 文件分类总览

### 1.1 需要移植的核心文件 ✅

这些文件包含核心业务逻辑，需要完整移植到服务器项目：

```
include/
├── ww_log_ram.h          ✅ 需要移植
├── ww_log_storage.h      ✅ 需要移植
├── ww_log_header.h       ✅ 需要移植
├── ww_log_cmd.h          ✅ 需要移植
├── ww_log_parser.h       ✅ 需要移植
└── ww_log_config.h       ✅ 需要移植（需修改配置）

core/
├── ww_log_ram.c          ✅ 需要移植
├── ww_log_storage.c      ✅ 需要移植
├── ww_log_header.c       ✅ 需要移植
├── ww_log_flush.c        ✅ 需要移植
├── ww_log_cmd.c          ✅ 需要移植
└── ww_log_parser.c       ✅ 需要移植
```

### 1.2 仿真专用文件 ❌

这些文件仅用于PC端仿真，**不需要**移植到服务器项目：

```
sim/
├── sim_hardware.h        ❌ 仿真专用
├── sim_hardware.c        ❌ 仿真专用
├── sim_storage.h         ❌ 仿真专用
├── sim_storage.c         ❌ 仿真专用
├── sim_config.h          ❌ 仿真专用
├── sim_config.c          ❌ 仿真专用
├── sim_config.json       ❌ 仿真专用
└── sim_data/             ❌ 仿真专用
    ├── eeprom.bin
    └── flash.bin

src/test/
├── test_ram_buffer.c     ❌ 仿真测试（可选移植）
├── test_storage.c        ❌ 仿真测试（可选移植）
└── test_*.c              ❌ 仿真测试（可选移植）
```

---

## 2. 代码中的条件编译标记

### 2.1 仿真模式标记

所有仿真相关代码都用 `SIMULATION_MODE` 宏包裹：

```c
#ifdef SIMULATION_MODE
    // 仿真代码 - 不需要移植
    static U8 g_sim_dlm_memory[4096];
    #define DLM_MAINTAIN_LOG_BASE_ADDR ((U32)g_sim_dlm_memory)
#else
    // 实际硬件代码 - 需要移植
    extern U8 __dlm_log_start;
    #define DLM_MAINTAIN_LOG_BASE_ADDR ((U32)&__dlm_log_start)
#endif
```

### 2.2 移植时的处理

**在服务器项目中：**
1. **不定义** `SIMULATION_MODE` 宏
2. 删除或注释掉所有 `#ifdef SIMULATION_MODE` 块中的代码
3. 保留 `#else` 块中的实际硬件代码

---

## 3. 需要适配的接口

### 3.1 RAM地址定义

**仿真代码（不移植）：**
```c
#ifdef SIMULATION_MODE
static U8 g_sim_dlm_memory[4096];
#define DLM_MAINTAIN_LOG_BASE_ADDR  ((U32)g_sim_dlm_memory)
#define DLM_MAINTAIN_LOG_SIZE       4096
#endif
```

**服务器项目需要提供：**
```c
// 在链接脚本或头文件中定义
#define DLM_MAINTAIN_LOG_BASE_ADDR  0x20001000  // 实际地址
#define DLM_MAINTAIN_LOG_SIZE       4096
```

### 3.2 外存类型检测

**仿真代码（不移植）：**
```c
#ifdef SIMULATION_MODE
EXT_MEM_TYPE_E log_storage_detect_type(void) {
    return g_sim_sys_info.extMemType;  // 从配置文件读取
}
#endif
```

**服务器项目需要实现：**
```c
EXT_MEM_TYPE_E log_storage_detect_type(void) {
    // 从实际寄存器读取
    return (EXT_MEM_TYPE_E)REG_WW_STUS_SYS_INFO_U.sub.extMemType;
}
```

### 3.3 分区表接口

**仿真代码（不移植）：**
```c
#ifdef SIMULATION_MODE
PART_TABLE_T* pt_info_read(void) {
    return sim_pt_info_read();  // 从JSON加载
}
#endif
```

**服务器项目需要实现：**
```c
PART_TABLE_T* pt_info_read(void) {LAUNCH_INFO_T *pLaunchInfo = dlm_data_launch_info_get();
    return &pLaunchInfo->pt_info;
}
```

### 3.4 外存读写接口

**仿真代码（不移植）：**
```c
#ifdef SIMULATION_MODE
int svc_eeprom_acc_write(U32 offset, const U8 *data, U32 size) {
    return sim_eeprom_write(offset, data, size);  // 文件操作
}
#endif
```

**服务器项目需要实现：**
```c
// 使用实际的SVC接口
#include "svc_ex.h"
// svc_eeprom_acc_write() 已经存在
// svc_flash_acc_write() 已经存在
```

---

## 4. 移植步骤

### Step 1: 准备工作

1. **确认服务器项目环境**
   - [ ] DLM内存区域已分配
   - [ ] 外存驱动已实现（EEPROM/Flash）
   - [ ] 分区表已配置
   - [ ] 相关头文件可访问

2. **创建目录结构**
   ```
   server_project/
   ├── log/
   │   ├── include/
   │   └── src/
   ```

### Step 2: 复制核心文件

```bash
# 复制头文件
cp include/ww_log_ram.h      server_project/log/include/
cp include/ww_log_storage.h  server_project/log/include/
cp include/ww_log_header.h   server_project/log/include/
cp include/ww_log_cmd.h      server_project/log/include/
cp include/ww_log_parser.h   server_project/log/include/

# 复制实现文件
cp core/ww_log_ram.c         server_project/log/src/
cp core/ww_log_storage.c     server_project/log/src/
cp core/ww_log_header.c      server_project/log/src/
cp core/ww_log_flush.c       server_project/log/src/
cp core/ww_log_cmd.c         server_project/log/src/
cp core/ww_log_parser.c      server_project/log/src/
```

### Step 3: 修改配置文件

**ww_log_config.h** 需要根据实际硬件修改：

```c
// ========== 仿真模式开关 ==========
// 移植时注释掉或删除这一行
// #define SIMULATION_MODE

// ========== RAM地址定义 ==========
#ifndef SIMULATION_MODE
// 使用实际硬件地址
#define DLM_MAINTAIN_LOG_BASE_ADDR  0x20001000  // 根据实际修改
#define DLM_MAINTAIN_LOG_SIZE       4096
#endif

// ========== 外存配置 ==========
#define LOG_STORAGE_PARTITION_SIZE  4096
#define LOG_RAM_FLUSH_THRESHOLD     3072  // 3KB

// ========== 其他配置 ==========
// 根据实际需求调整...
```

### Step 4: 清理仿真代码

在每个移植的文件中，删除或注释掉所有 `#ifdef SIMULATION_MODE` 块：

```c
// 删除这样的代码块
#ifdef SIMULATION_MODE
    // ... 仿真代码 ...
#endif

// 保留这样的代码块
#ifndef SIMULATION_MODE
    // ... 实际硬件代码 ...
#endif
```

### Step 5: 实现硬件接口

在 `ww_log_storage.c` 中实现实际的硬件接口：

```c
// 实现外存类型检测
EXT_MEM_TYPE_E log_storage_detect_type(void) {
    return (EXT_MEM_TYPE_E)REG_WW_STUS_SYS_INFO_U.sub.extMemType;
}

// 实现分区表读取
PART_TABLE_T* log_storage_get_partition_table(void) {
    return pt_info_read();
}

// 实现分区表验证
U8 log_storage_check_partition_valid(PART_TABLE_T *pt) {
    return pt_table_check_valid(pt);
}

// 外存读写接口直接使用SVC接口
// svc_eeprom_acc_write()
// svc_eeprom_acc_read()
// svc_flash_acc_write()
// svc_flash_acc_read()
```

### Step 6: 集成到构建系统

**Makefile 修改：**
```makefile
# 添加LOG Phase2源文件
LOG_PHASE2_SRCS = \
    log/src/ww_log_ram.c \
    log/src/ww_log_storage.c \
    log/src/ww_log_header.c \
    log/src/ww_log_flush.c \
    log/src/ww_log_cmd.c \
    log/src/ww_log_parser.c

# 添加头文件路径
INCLUDES += -Ilog/include

# 添加到总的源文件列表
SRCS += $(LOG_PHASE2_SRCS)
```

### Step 7: 测试验证

1. **编译测试**
   ```bash
   make clean
   make all
   ```

2. **功能测试**
   - 测试LOG写入到RAM
   - 测试自动刷新触发
   - 测试外存读写
   - 测试命令行接口

3. **压力测试**
   - 连续写入大量LOG
   - 验证数据完整性
   - 检查内存使用

---

## 5. 关键差异对照表

| 功能 | 仿真实现 | 服务器项目实现 |
|------|----------|----------------|
| RAM地址 | 静态数组 `g_sim_dlm_memory` | 链接脚本定义的实际地址 |
| 外存类型检测 | 配置文件 | 寄存器 `REG_WW_STUS_SYS_INFO_U` |
| 分区表读取 | JSON文件 | `pt_info_read()` 函数 |
| EEPROM读写 | 文件操作 | `svc_eeprom_acc_*()` |
| Flash读写 | 文件操作 | `svc_flash_acc_*()` |
| 时间戳 | `time()` | 实际RTC或系统时钟 |

---

## 6. 注意事项

### 6.1 内存对齐

确保RAM缓冲区地址是4字节对齐的：
```c
// 在链接脚本中
.dlm_log_section (NOLOAD) : ALIGN(4) {
    __dlm_log_start = .;
    . = . + 4096;
    __dlm_log_end = .;
} > DLM_RAM
```

### 6.2 中断安全

如果在中断中使用LOG，需要确保临界区保护：
```c
// 在 ww_log_ram.c 中
#define ENTER_CRITICAL()  __disable_irq()
#define EXIT_CRITICAL()   __enable_irq()
```

### 6.3 Flash擦除

Flash写入前必须擦除，确保 `svc_flash_acc_write()` 已经处理了擦除：
```c
// 如果SVC接口没有自动擦除，需要手动调用
svc_flash_acc_erase(offset, 4096);
svc_flash_acc_write(offset, data, size);
```

### 6.4 分区表格式

确认服务器项目的分区表格式与设计一致：
```c
typedef struct {
    U32 part_offset;    // 分区起始地址
    U32 part_size;      // 分区大小
    U8  part_type;      // 分区类型（LOG=?）
    U8  disk_type;      // 存储类型
    // ... 其他字段
} PART_ENTRY_T;
```

---

## 7. 移植检查清单

### 编译阶段
- [ ] 所有源文件编译通过
- [ ] 没有 `SIMULATION_MODE` 相关的编译错误
- [ ] 链接成功，没有未定义的符号
- [ ] 代码大小在预期范围内（<5KB增量）

### 功能测试
- [ ] LOG可以写入RAM
- [ ] RAM使用量统计正确
- [ ] 达到3KB时自动触发刷新
- [ ] 手动刷新命令工作正常
- [ ] 外存类型检测正确
- [ ] 分区表读取成功
- [ ] 数据可以写入外存
- [ ] 数据可以从外存读取
- [ ] 所有6个命令正常工作
- [ ] LOG解析输出正确

### 稳定性测试
- [ ] 长时间运行无崩溃
- [ ] 内存无泄漏
- [ ] 断电重启后数据恢复正常
- [ ] 外存写入失败时正确处理
- [ ] Ring Buffer翻转正常

---

## 8. 常见问题

### Q1: 编译时提示找不到 `sim_*` 函数？
**A:** 检查是否定义了 `SIMULATION_MODE`，移植时不应该定义这个宏。

### Q2: 链接时提示 `__dlm_log_start` 未定义？
**A:** 需要在链接脚本中定义DLM内存区域。

### Q3: 运行时外存类型检测失败？
**A:** 检查 `REG_WW_STUS_SYS_INFO_U` 寄存器是否正确初始化。

### Q4: 分区表读取返回NULL？
**A:** 检查 `pt_info_read()` 函数是否正确实现，以及分区表是否已初始化。

### Q5: Flash写入失败？
**A:** 确认写入前已经擦除，且地址在有效范围内。

---

## 9. 技术支持

如果在移植过程中遇到问题，可以：

1. **查看仿真代码**：参考 `sim/` 目录下的实现
2. **对比差异**：使用本文档的对照表
3. **单步调试**：在关键函数设置断点
4. **日志输出**：添加调试LOG跟踪执行流程

---

## 10. 总结

### 移植要点
✅ **需要移植**：`include/` 和 `core/` 下的核心文件
❌ **不需要移植**：`sim/` 和 `src/test/` 下的仿真文件
🔧 **需要适配**：RAM地址、外存接口、分区表接口
⚙️ **需要配置**：`ww_log_config.h` 中的硬件相关配置

### 移植流程
1. 复制核心文件
2. 删除仿真代码
3. 实现硬件接口
4. 修改配置文件
5. 集成到构建系统
6. 测试验证

### 预期结果
- 代码增量：~5KB
- 功能完整：所有Phase2功能可用
- 性能良好：LOG写入<10μs
- 稳定可靠：长时间运行无问题

---

**文档版本：v1.0**
**创建日期：2025-12-24**
**适用版本：Phase2**
