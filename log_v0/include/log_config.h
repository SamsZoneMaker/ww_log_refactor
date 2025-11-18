#ifndef LOG_CONFIG_H
#define LOG_CONFIG_H

// 模式选择
#define LOG_MODE_STR 0
#define LOG_MODE_ECODE 1
#define LOG_MODE LOG_MODE_ECODE  // 静态选择，调试时用STR，量产用ECODE

// 位分配宏
#define FILE_ID_BITS 12
#define LINE_BITS 12
#define DATA_LEN_BITS 8

// 最大值
#define MAX_DATA_LEN ((1 << DATA_LEN_BITS) - 1)  // 255

// 位移
#define FILE_ID_SHIFT (LINE_BITS + DATA_LEN_BITS)
#define LINE_SHIFT DATA_LEN_BITS
#define DATA_LEN_SHIFT 0

#endif