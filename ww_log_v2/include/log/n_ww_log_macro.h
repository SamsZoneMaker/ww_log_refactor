/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* ww_log call-site macros: the N_LOG_ERR/WRN/INF/DBG family (mode-dispatched to
 * string / encode / disabled) and the N_*_IF_TRUE return-code helpers built on
 * top of N_LOG_ERR. Include this header in any .c that logs. Depends on
 * n_ww_log_def.h (levels, thresholds, encode/CURRENT_* macros) and
 * n_ww_log_output.h (the output function declarations). */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_MACRO_H__
#define __N_WW_LOG_MACRO_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "n_ww_log_def.h"
#include "n_ww_log_output.h"

/*************************** macro definition start ***************************/

#if defined(CONFIG_N_LOG) && \
    (CONFIG_N_LOG_MODE == N_WW_LOG_MODE_STRING) && \
    defined(CONFIG_N_LOG_BACKEND_UART)

#ifdef __NOTDIR_FILE__
#define __WW_LOG_FILENAME__(path)    __NOTDIR_FILE__
#else
#define __WW_LOG_FILENAME__(path) \
    (strrchr(path, '/') ? strrchr(path, '/') + 1 : \
    (strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path))
#endif

/* Static-switch conditional expansion */
#define __WW_LOG_STR_IF_0(...)    do { } while (0)
#define __WW_LOG_STR_IF_1(...)    __VA_ARGS__
#define __WW_LOG_STR_CAT(a, b)    __WW_LOG_STR_CAT_IMPL(a, b)
#define __WW_LOG_STR_CAT_IMPL(a, b)    a##b
#define __WW_LOG_STR_IF(cond)     __WW_LOG_STR_CAT(__WW_LOG_STR_IF_, cond)

#define __WW_LOG_STR_CALL(level, fmt, ...) \
    n_ww_log_str_output(CURRENT_MODULE_ID, __WW_LOG_FILENAME__(__FILE__), __LINE__, \
                        level, fmt, ##__VA_ARGS__)

#define __WW_LOG_STR_STATIC_EXPAND(level, fmt, ...) \
    __WW_LOG_STR_IF(CURRENT_MODULE_STATIC_EN)(__WW_LOG_STR_CALL(level, fmt, ##__VA_ARGS__))

#if (CONFIG_N_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_ERR)
#define N_LOG_ERR(fmt, ...)    __WW_LOG_STR_STATIC_EXPAND(N_WW_LOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#else
#define N_LOG_ERR(fmt, ...)    do { } while (0)
#endif

#if (CONFIG_N_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_WRN)
#define N_LOG_WRN(fmt, ...)    __WW_LOG_STR_STATIC_EXPAND(N_WW_LOG_LEVEL_WRN, fmt, ##__VA_ARGS__)
#else
#define N_LOG_WRN(fmt, ...)    do { } while (0)
#endif

#if (CONFIG_N_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_INF)
#define N_LOG_INF(fmt, ...)    __WW_LOG_STR_STATIC_EXPAND(N_WW_LOG_LEVEL_INF, fmt, ##__VA_ARGS__)
#else
#define N_LOG_INF(fmt, ...)    do { } while (0)
#endif

#if (CONFIG_N_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_DBG)
#define N_LOG_DBG(fmt, ...)    __WW_LOG_STR_STATIC_EXPAND(N_WW_LOG_LEVEL_DBG, fmt, ##__VA_ARGS__)
#else
#define N_LOG_DBG(fmt, ...)    do { } while (0)
#endif


#elif defined(CONFIG_N_LOG) && \
      (CONFIG_N_LOG_MODE == N_WW_LOG_MODE_ENCODE)

/* Variadic argument counter (0-16) */
#define __WW_LOG_ARG_COUNT(...) \
    __WW_LOG_ARG_COUNT_IMPL(0, ##__VA_ARGS__, \
        16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define __WW_LOG_ARG_COUNT_IMPL( \
    _0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N, ... ) N

/* Static-switch conditional expansion */
#define __WW_LOG_IF_0(...)
#define __WW_LOG_IF_1(...)        __VA_ARGS__
#define __WW_LOG_CAT(a, b)        __WW_LOG_CAT_IMPL(a, b)
#define __WW_LOG_CAT_IMPL(a, b)   a##b
#define __WW_LOG_IF(cond)         __WW_LOG_CAT(__WW_LOG_IF_, cond)

#define __WW_LOG_ENCODE_CALL(level, fmt, ...) \
    n_ww_log_encode_output(CURRENT_FILE_ID, __LINE__, level, \
                           __WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)

#define __WW_LOG_STATIC_EXPAND(level, fmt, ...) \
    __WW_LOG_IF(CURRENT_MODULE_STATIC_EN)(__WW_LOG_ENCODE_CALL(level, fmt, ##__VA_ARGS__))

#if (CONFIG_N_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_ERR)
#define N_LOG_ERR(fmt, ...)    __WW_LOG_STATIC_EXPAND(N_WW_LOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#else
#define N_LOG_ERR(fmt, ...)    do { } while (0)
#endif

#if (CONFIG_N_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_WRN)
#define N_LOG_WRN(fmt, ...)    __WW_LOG_STATIC_EXPAND(N_WW_LOG_LEVEL_WRN, fmt, ##__VA_ARGS__)
#else
#define N_LOG_WRN(fmt, ...)    do { } while (0)
#endif

#if (CONFIG_N_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_INF)
#define N_LOG_INF(fmt, ...)    __WW_LOG_STATIC_EXPAND(N_WW_LOG_LEVEL_INF, fmt, ##__VA_ARGS__)
#else
#define N_LOG_INF(fmt, ...)    do { } while (0)
#endif

#if (CONFIG_N_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_DBG)
#define N_LOG_DBG(fmt, ...)    __WW_LOG_STATIC_EXPAND(N_WW_LOG_LEVEL_DBG, fmt, ##__VA_ARGS__)
#else
#define N_LOG_DBG(fmt, ...)    do { } while (0)
#endif

#else  /* neither STRING nor ENCODE -> DISABLED: all LOG macros are no-ops. */
#define N_LOG_ERR(...)    do { } while (0)
#define N_LOG_WRN(...)    do { } while (0)
#define N_LOG_INF(...)    do { } while (0)
#define N_LOG_DBG(...)    do { } while (0)

#endif /* mode selection */


/* ========= return-code helpers (built on N_LOG_ERR) ========= */
#define N_RETURN_CODE_IF_TRUE(vExpression, rc) \
{ \
    if(vExpression) \
    { \
        N_LOG_ERR("rc:0x%x\r\n", rc); \
        return rc; \
    } \
}

#define N_RETURN_IF_TRUE(vExpression, rc) \
{ \
    if(vExpression) \
    { \
        N_LOG_ERR("rc:0x%x\r\n", rc); \
        return; \
    } \
}

#define N_BREAK_IF_TRUE(vExpression, rc) \
{ \
    if(vExpression) \
    { \
        N_LOG_ERR("rc:0x%x\r\n", rc); \
        break; \
    } \
}

#define N_CONTINUE_IF_TRUE(vExpression, rc) \
{ \
    if(vExpression) \
    { \
        N_LOG_ERR("rc:0x%x\r\n", rc); \
        continue; \
    } \
}

#define N_PRINT_IF_TRUE(vExpression, rc) \
{ \
    if(vExpression) \
    { \
        N_LOG_ERR("rc:0x%x\r\n", rc); \
    } \
}

#define N_BREAK_IF_TRUE_WO_PRINT(vExpression) \
{ \
    if(vExpression) \
    { \
        break; \
    } \
}

#define N_RETURN_IF_TRUE_WO_PRINT(vExpression, rc) \
{ \
    if(vExpression) \
    { \
        return; \
    } \
}

#define N_RETURN_CODE_IF_TRUE_WO_PRINT(vExpression, rc) \
{ \
    if(vExpression) \
    { \
        return rc; \
    } \
}

#define N_CONTINUE_IF_TRUE_WO_PRINT(vExpression, rc) \
{ \
    if(vExpression) \
    { \
        continue; \
    } \
}

#define N_GOTO_FLAG_IF_TRUE_WO_PRINT(vExpression, vFlag) \
{ \
    if (vExpression) \
    { \
        goto vFlag; \
    } \
}

/*************************** macro definition end *****************************/

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_MACRO_H__ */
