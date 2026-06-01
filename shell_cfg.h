/**
 * @file    shell_cfg.h
 * @brief   Shell配置 - 精简CLI版本
 */

#ifndef SHELL_CFG_H_
#define SHELL_CFG_H_

/* 基础配置 */
#define SHELL_CMD_SIZE    128  /* 命令缓冲区大小 */
#define SHELL_ARG_MAX     16   /* 最大参数数量 */
#define SHELL_PROMPT      "> " /* 命令提示符 */
#define SHELL_PRINTF_SIZE 128  /* printf单次最大输出 (栈上分配) */

/* 接收缓冲区配置 (环形缓冲区, 0=不使用内置缓冲) */
#define SHELL_RX_BUF_SIZE 64 /* 接收环形缓冲区大小, 必须是2的幂 */

/* 编译时校验: SHELL_RX_BUF_SIZE 必须是2的幂 */
#if SHELL_RX_BUF_SIZE > 0 && (SHELL_RX_BUF_SIZE & (SHELL_RX_BUF_SIZE - 1)) != 0
    #error "SHELL_RX_BUF_SIZE must be a power of 2"
#endif

/* 功能开关 (可通过编译器 -D 选项覆盖) */
#ifndef SHELL_USING_CMD_EXPORT
    #define SHELL_USING_CMD_EXPORT 1 /* 使用宏导出命令 (需链接脚本支持) */
#endif
#ifndef SHELL_USING_VAR
    #define SHELL_USING_VAR 1 /* 变量读写功能 */
#endif
#ifndef SHELL_USING_HISTORY
    #define SHELL_USING_HISTORY 1 /* 历史记录 */
#endif
#ifndef SHELL_USING_COMPLETION
    #define SHELL_USING_COMPLETION 1 /* Tab补全 */
#endif
#ifndef SHELL_USING_PASSTHROUGH
    #define SHELL_USING_PASSTHROUGH 1 /* 透传模式 */
#endif
#ifndef SHELL_USING_AUTH
    #define SHELL_USING_AUTH 1 /* 用户权限 */
#endif

/* 历史记录配置 */
#if SHELL_USING_HISTORY
    #define SHELL_HISTORY_MAX 8
#endif

/* 用户权限配置 */
#if SHELL_USING_AUTH
    #define SHELL_USER_MAX       4  /* 最大用户数量 */
#endif

/* 按键定义 */
#define KEY_ESC       0x1B
#define KEY_TAB       0x09
#define KEY_BS        0x08
#define KEY_DEL       0x7F
#define KEY_CR        0x0D
#define KEY_LF        0x0A
#define KEY_CTRL_C    0x03
#define KEY_CTRL_EXIT 0x1D /* Ctrl+] 退出透传 */

/* ==================== ISR日志队列配置 ==================== */
#ifndef SHELL_USING_LOG_QUEUE
    #define SHELL_USING_LOG_QUEUE 1  /* 启用ISR日志队列 */
#endif

#if SHELL_USING_LOG_QUEUE
    #define SHELL_LOG_QUEUE_SIZE 256  /* 日志队列大小，必须是2的幂 */
#endif

#endif
