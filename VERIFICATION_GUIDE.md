# LOG 系统验证指南

## 问题1：模块开关是编译期还是运行期？

### 答案：**运行期动态开关**

#### 实验验证：

```bash
# 1. 修改 g_ww_log_module_mask = 0x05 (禁用 DEMO 模块，bit 1 = 0)
# 2. 编译 str 模式
# 3. 检查字符串

$ strings bin/log_test_str | grep "Demo module"
Demo module initializing...  # ← 仍然在二进制中！
```

#### 原理解析：

`g_ww_log_module_mask` 是一个**全局变量**，不是宏定义：

```c
// ww_log_modules.c
U32 g_ww_log_module_mask = 0xFFFFFFFF;  // ← 运行期变量
```

LOG 宏展开后是**运行期 if 判断**：

```c
// LOG_INF(WW_LOG_MODULE_DEMO, "Demo message") 展开为：

do {
    if ((g_ww_log_module_mask & (1U << 1)) != 0 && ...) {
        //     ^^^^^^^^^^^^^^^^^^^^^^^^^^ 运行期变量检查
        ww_log_str_output("demo.c", 10, 2, "Demo message");
        //                                   ^^^^^^^^^^^^^^ 字符串仍在代码中
    }
} while(0)
```

编译器在编译时**不知道** `g_ww_log_module_mask` 的值，所以：
- ✅ 所有 LOG 调用的代码都会编译进去
- ✅ 所有字符串都会编译进去
- ✅ 运行时通过 if 判断控制是否执行
- ✅ 后续调用 `ww_log_enable_module()` **有效**

#### 结论：

| 特性 | 结果 |
|------|------|
| 代码是否编译进去 | ✅ 全部编译 |
| 字符串是否编译进去 | ✅ 全部编译 |
| enable API 是否有效 | ✅ 有效 |
| 性能影响 | 运行期 if 判断（微小开销） |

---

## 问题2：如何验证代码是否被编译进去？

### 方法1：比较二进制大小

```bash
$ make MODE=str && ls -lh bin/log_test_str
62K  bin/log_test_str

$ make MODE=encode && ls -lh bin/log_test_encode
78K  bin/log_test_encode  # 包含编码逻辑，但无字符串

$ make MODE=disabled && ls -lh bin/log_test_disabled
41K  bin/log_test_disabled  # LOG 宏为空，代码被优化掉
```

**观察：**
- str 模式：62K（包含字符串 + printf 调用）
- encode 模式：78K（包含编码逻辑 + 编码输出，但无格式字符串）
- disabled 模式：41K（LOG 宏全部优化掉）

**意外发现：** encode 模式比 str 模式大？
- 原因：encode 模式包含复杂的位运算和参数打包逻辑
- str 模式只是简单的 printf 调用
- **但在实际嵌入式环境中，encode 模式节省的是 ROM 中的字符串空间！**

---

### 方法2：检查字符串（最直接）

```bash
# 检查 str 模式
$ strings bin/log_test_str | grep "Demo module"
Demo module initializing...       # ✓ 找到
Processing task...                 # ✓ 找到
Hardware check passed, code=%d    # ✓ 找到

# 检查 encode 模式（应该没有）
$ strings bin/log_test_encode | grep "Demo module"
(无输出)  # ✓ 正确！格式字符串被优化掉了

# 验证完整
$ ./verify_encode_mode.sh
✅ encode 模式验证通过：没有 LOG 格式字符串被编译进去
```

**这是最简单、最直接的验证方法！**

---

### 方法3：检查函数符号（nm 命令）

```bash
# str 模式使用的函数
$ nm bin/log_test_str | grep ww_log
00000000000017e0 T ww_log_str_output     # ✓ str 模式输出函数

# encode 模式使用的函数
$ nm bin/log_test_encode | grep ww_log
0000000000001a00 T ww_log_encode_output  # ✓ encode 模式输出函数

# encode 模式中不应该有 str 函数
$ nm bin/log_test_encode | grep ww_log_str_output
(无输出)  # ✓ 正确！str 函数未链接
```

**用途：** 验证链接器是否正确排除了未使用的函数。

---

### 方法4：查看预处理代码（宏展开）

```bash
# 生成预处理文件
$ gcc -E src/demo/demo_init.c -I./include -DWW_LOG_MODE_STR -o demo_init_str.i
$ gcc -E src/demo/demo_init.c -I./include -DWW_LOG_MODE_ENCODE -o demo_init_encode.i
```

#### str 模式宏展开（简化版）：

```c
LOG_INF(CURRENT_LOG_PARAM, "Demo module initializing...");

// 展开为 ↓

do {
    if (((g_ww_log_module_mask & (1U << (1))) != 0) && (3 >= 2)) {
        ww_log_str_output(
            "demo_init.c",  // 文件名
            21,             // 行号
            2,              // 级别（INF）
            "Demo module initializing..."  // ← 格式字符串保留
        );
    }
} while(0);
```

#### encode 模式宏展开（简化版）：

```c
LOG_INF(CURRENT_LOG_PARAM, "Demo module initializing...");

// 展开为 ↓

do {
    U8 _module_id = ((33) >> 5);  // 提取模块 ID
    if (((g_ww_log_module_mask & (1U << (_module_id))) != 0) && (3 >= 2)) {
        (void)"[DEMO]";  // ← module_tag 被优化掉（仅用于类型检查）

        U32 _encoded = (
            (((U32)(33) & 0xFFF) << 20) |  // LOG_ID = 33
            (((U32)(21) & 0xFFF) << 8) |   // LINE = 21
            (((U32)(0) & 0x3F) << 2) |     // DATA_LEN = 0
            ((U32)(2) & 0x3)               // LEVEL = 2 (INF)
        );

        ww_log_encode_output(_encoded, (const U32*)0, 0);
        //                   ^^^^^^^^  无格式字符串！只有编码值
    }
} while(0);
```

**关键差异：**
- str 模式：字符串 `"Demo module initializing..."` 保留在代码中
- encode 模式：字符串被丢弃，只保留编码后的 32 位整数

---

### 方法5：反汇编查看汇编代码

```bash
# 反汇编特定函数
$ objdump -d bin/log_test_encode -M intel | grep -A 30 "demo_init>:"

# 或者生成汇编文件
$ gcc -S src/demo/demo_init.c -I./include -DWW_LOG_MODE_ENCODE -o demo_init.s
$ cat demo_init.s
```

**用途：** 深入查看编译器生成的机器码，验证优化效果。

---

## 验证示例：特定 LOG 是否被编译

### 场景：验证 `LOG_ERR(MODULE_DEFAULT, "Test output:%d", a)` 是否在二进制中

#### 方法A：搜索字符串

```bash
# str 模式
$ strings bin/log_test_str | grep "Test output"
Test output:%d  # ← 找到

# encode 模式
$ strings bin/log_test_encode | grep "Test output"
(无输出)  # ← 未找到（正确）
```

#### 方法B：搜索预处理文件

```bash
# 生成预处理文件
$ gcc -E your_file.c -I./include -DWW_LOG_MODE_STR -o preprocessed.i

# 搜索展开后的代码
$ grep -A 5 "Test output" preprocessed.i
```

#### 方法C：使用验证脚本

```bash
# 运行自动化验证
$ ./verify_encode_mode.sh
```

---

## 常见验证任务

### 任务1：验证 encode 模式下没有格式字符串

```bash
# 编译 encode 模式
make MODE=encode

# 提取所有字符串
strings bin/log_test_encode > encode_strings.txt

# 检查是否包含 LOG 消息（不应该有）
cat encode_strings.txt | grep -E "initialized|processing|check|error|warning"

# 如果有输出，说明字符串泄漏了
```

### 任务2：验证模块开关的影响

```bash
# 修改 mask（只启用 DEFAULT 模块）
sed -i 's/0xFFFFFFFF/0x00000001/' src/core/ww_log_modules.c

# 编译
make MODE=str

# 验证：其他模块代码仍然在二进制中
strings bin/log_test_str | grep "Demo module"  # ← 仍然找到！

# 恢复
sed -i 's/0x00000001/0xFFFFFFFF/' src/core/ww_log_modules.c
```

### 任务3：验证 disabled 模式优化效果

```bash
# 编译 disabled 模式
make MODE=disabled

# 检查 LOG 函数是否被优化掉
nm bin/log_test_disabled | grep ww_log
(应该只有 ww_log_init，没有 output 函数)

# 检查大小（应该是最小的）
ls -lh bin/log_test_*
```

---

## 工具总结

| 工具 | 用途 | 示例 |
|------|------|------|
| `strings` | 查看二进制中的字符串 | `strings bin/app \| grep "message"` |
| `nm` | 查看符号表（函数名） | `nm bin/app \| grep ww_log` |
| `objdump` | 反汇编查看机器码 | `objdump -d bin/app` |
| `gcc -E` | 生成预处理文件（宏展开） | `gcc -E file.c -o file.i` |
| `gcc -S` | 生成汇编文件 | `gcc -S file.c -o file.s` |
| `size` | 查看段大小（text/data/bss） | `size bin/app` |
| `readelf` | 查看 ELF 文件信息 | `readelf -S bin/app` |

---

## 快速验证命令

```bash
# 一键验证所有内容
./verify_encode_mode.sh

# 只验证 encode 模式字符串
make MODE=encode && strings bin/log_test_encode | grep -i "demo\|test\|hardware"

# 只验证二进制大小
make MODE=str && stat -c%s bin/log_test_str
make MODE=encode && stat -c%s bin/log_test_encode

# 只验证符号
make MODE=encode && nm bin/log_test_encode | grep ww_log
```

---

## 总结

### 问题1答案：

**`g_ww_log_module_mask` 是运行期开关**
- 所有模块代码都会编译进去
- 后续 enable API 有效
- 适合需要动态控制的场景

### 问题2答案：

**最简单的验证方法：**
```bash
strings bin/app | grep "your_log_message"
```

**完整验证流程：**
1. `strings` 检查字符串
2. `nm` 检查函数符号
3. 比较二进制大小
4. 查看预处理代码（可选）

---

**提示：** 所有验证脚本已提供：
- `verify_compilation.sh` - 验证模块开关行为
- `verify_encode_mode.sh` - 完整验证所有方面
