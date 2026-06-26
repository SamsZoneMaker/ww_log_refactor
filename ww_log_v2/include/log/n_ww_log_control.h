/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* to add description for this file if needed */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_CONTROL_H__
#define __N_WW_LOG_CONTROL_H__

#ifdef __cplusplus
extern "C"
{
#endif

// #include <>
// #include ""
#include "ww_type.h"
#include "n_ww_log_output.h"

/*************************** macro definition start ***************************/

/* ========= Log Levels ========= */
#define N_WW_LOG_LEVEL_ERR    0  /* Error: failures, critical issues */
#define N_WW_LOG_LEVEL_WRN    1  /* Warning: potential problems */
#define N_WW_LOG_LEVEL_INF    2  /* Info: important state changes */
#define N_WW_LOG_LEVEL_DBG    3  /* Debug: detailed execution flow */

/**
 * Compile-time level threshold. Logs with level > threshold are compiled
 * out entirely (zero code size). Override via -DN_WW_LOG_COMPILE_THRESHOLD=n.
 */
#ifndef N_WW_LOG_COMPILE_THRESHOLD
#define N_WW_LOG_COMPILE_THRESHOLD    N_WW_LOG_LEVEL_DBG
#endif

#define N_WW_LOG_MODULE_MAX    32

#define N_RETURN_CODE_IF_TRUE(vExpression, rc) \
{ \
    if(vExpression) \
    { \
        N_LOG_ERR("-- line:%d rc:0x%x\r\n", __LINE__, rc); \
        return rc; \
    } \
}

#define N_RETURN_IF_TRUE(vExpression, rc) \
{ \
    if(vExpression) \
    { \
        N_LOG_ERR("-- line:%d rc:0x%x\r\n", __LINE__, rc); \
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

/*************************** type definition start ***************************/
/*************************** type definition end *****************************/


/*************************** declaration start ***************************/
extern U32 g_ww_log_module_mask;
extern U8 g_ww_log_level_threshold;

void n_ww_log_init(void);

void n_ww_log_set_level_threshold(U8 level);
U8 n_ww_log_get_level_threshold(void);

void n_ww_log_set_module_mask(U32 mask);
U32 n_ww_log_get_module_mask(void);

void n_ww_log_enable_module(U8 module_id);
void n_ww_log_disable_module(U8 module_id);
WW_BOOL n_ww_log_is_module_enabled(U8 module_id);

/*************************** declaration end *****************************/

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_CONTROL_H__ */
