## 设计要求

### 总体目标

1. 节省code_size
2. 节省ram_size
3. 准确记录关键信息



### 当前情况

- 当前已经存在一种日志模块称之为str_mode,类似如下接口

  以string方式记录主要信息，占用比较大的code_size

  ```c
  #define TEST_LOG_INFO_MSG(fmt, ...)  printf("[INFO] %s:%d:%s: " fmt "\n",  __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
  ```

- 要新设计一种日志模块成为encode_mode



### 总体需求

1. 无论是str_mode还是encode，用的都是统一的接口，例如上面提到的 `TEST_LOG_INFO_MSG`

2. 模式的选择

   二选一，或者都不选不输出信息到日志文件中

   当选择str_mode的时候，和encode_code相关的代码不能影响到code_size，反之亦然

3. 动态开关和静态开关

   - 静态开关

     编译时的开关，主要是进一步降低code_size

   - 动态开关

     运行时的开关，主要是减少log的记录量



### Encode_mode

1. 临界区域的保护

   多任务写日志，会引起日志的混乱

2. encode_mode相关的features

   - 是否输出到uart
   - 是否保存到ram
     - 总条数可配置
     - 热重启后需要存在
     - 不覆盖，除非已被取走，最主要考虑的是error的message
     - 支持清空
   - 是否需要保存到外村？eeprom/flash/none
     - 支持保存到ram是支持
     - 支持情况

3. rsdk可获取log

   要求rsdk可以获取ram中或者外存中的log

4. 关键的数据结构

   WW_LOG_RAM_T

   ```C
   typedef struct
   {
   	U16 logEntryHead;
   	U16 logEntryTail;
   	U32 logEntry[WW_LOG_RAM_ENTRY_NUM];
   	U16 logMedia;
   	...
   }WW_LOG_RAM_T
   ```

   说明

   - logEntryHead == logEntryTail为空，(logEntryTail + 1) == logEntryHead 为满，要注意翻转
   - 放入时，logEntryTail++，注意翻转
   - 取出时，logEntryHead++，注意翻转
   - logMedia为外存中的log数量

   

   编码方案

   - 每个文件一个编号
   - 编号定义在一个统一的位置，全局唯一
   - 要输出行数
   - 可以考虑文件编号12bit，行数12bit（单个文件支持4095行)

   

### 使用场景（just for reference example）

- `log_only(U32)`
- `log_data_1(U32, U32)`
- `log_data_2(U32, U32 *, U32)`



   

