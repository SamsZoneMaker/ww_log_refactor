# ww_log_refactor

嵌入式系统日志模块重构项目 - 支持 String 和 Encode 两种模式

## 快速开始

```bash
# String 模式（人类可读）
make test-str

# Encode 模式（二进制编码，最小资源占用）
make test-encode

# 解码二进制日志
./bin/log_test_encode | grep "^0x" | python3 tools/log_decoder.py -
```

## 核心特性

- ✅ **双模式支持**: String 模式（调试）/ Encode 模式（生产）
- ✅ **统一 API**: 两种模式使用相同的日志宏
- ✅ **模块化设计**: 模块级 + 文件级 ID 管理
- ✅ **参数记录**: Encode 模式记录所有参数值
- ✅ **零运行时开销**: 编译时模式选择
- ✅ **资源高效**: 代码减少 60-75%，RAM 减少 50-75%

## Encode 模式示例

**源代码**:
```c
LOG_DBG("[DRV]", "I2C write to device 0x%02X, data=0x%02X", 0x50, 0xAB);
```

**输出**:
```
0x0830270B 0x00000050 0x000000AB
```

**解码**:
```
[DBG][DRV] drv_i2c.c:39 Params:[0x00000050, 0x000000AB]
```

## 文档

- 📖 [实现说明](IMPLEMENTATION.md) - 详细的实现文档（中文）
- 📖 [AI 助手指南](CLAUDE.md) - AI 开发指南
- 📖 [设计需求](doc/log模块更新设计要求.md) - 原始需求（中文）
- 📖 [设计方案](doc/日志模块重构设计方案.md) - 详细设计（中文）

## 目录结构

```
ww_log_refactor/
├── include/          # 头文件（file_id.h, ww_log*.h）
├── src/              # 源文件（core, demo, brom, test, app, drivers）
├── tools/            # 工具（log_decoder.py）
├── examples/         # 测试程序
└── Makefile          # 构建系统
```

## 许可

本项目用于嵌入式系统日志系统设计参考。
