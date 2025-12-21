/*****************************************************************************
 * @file        shell_cfg.h
 * @brief       Shell配置文件 - 所有功能开关和参数配置
 * @author      liu
 * @date        2025-12-09
 * @version     2.0
 * @copyright   Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 * 
 * @details     本文件包含Shell系统的所有配置选项，通过修改宏定义可以裁剪功能
 *              以适应不同的硬件资源和应用需求。
 *****************************************************************************/

/* ===========================================================================
 *                           基础功能配置区
 * ===========================================================================*/

/**
 * Shell基本缓冲区配置
 * - SHELL_CMD_SIZE: 单条命令最大长度，建议范围 64~256
 * - SHELL_ARG_MAX: 单条命令最大参数个数，建议范围 8~32
 */

#ifndef __SHELL_CFG_H
#define __SHELL_CFG_H

#define SHELL_CMD_SIZE                  128         /* 命令缓冲区大小(字节) */
#define SHELL_ARG_MAX                   16          /* 最大参数数量 */

/**
 * Shell实例管理配置
 * - SHELL_MAX_INSTANCES: 支持的Shell实例数，每个实例对应一个终端
 * - 内存占用 ≈ SHELL_MAX_INSTANCES × sizeof(shell_t*)
 */
#define SHELL_MAX_INSTANCES             4           /* 最大Shell实例数(支持多终端) */

/**
 * Shell提示符配置
 * - 可以自定义为 "$ ", ">> ", "[shell]# " 等
 */
#define SHELL_PROMPT                    "shell> "   /* 默认命令提示符 */

/* ===========================================================================
 *                           功能裁剪开关区
 * ===========================================================================*/

/**
 * @name 核心功能开关
 * @{
 */
#define SHELL_USING_COMPLETION          1           /* 启用Tab命令补全功能(节省约500字节) */
#define SHELL_USING_HISTORY             1           /* 启用历史记录功能(节省约1KB) */
#define SHELL_USING_HELP                1           /* 启用help命令(节省约200字节) */
#define SHELL_USING_CMD_EXPORT          1           /* 启用命令自动导出功能 */
/** @} */

/**
 * @name 高级功能开关
 * @{
 */
#define SHELL_USING_AUTH                0           /* 启用用户权限管理(节省约2KB) */
#define SHELL_USING_PASSTHROUGH         0           /* 启用透传模式(节省约300字节) */
#define SHELL_USING_FUNC_SIGNATURE      1           /* 启用函数签名适配功能(节省约600字节) */
#define SHELL_USING_MULTI_INSTANCE      0           /* 启用多Shell实例管理(节省约200字节) */
#define SHELL_USING_COLOR               0           /* 启用ANSI彩色输出 */
/** @} */

/**
 * @name 调试和日志功能（新版已删除）
 * @{
 */
#define SHELL_SUPPORT_END_LINE          1           /* 启用日志输出时保持提示符 */
#define SHELL_SHOW_INFO                 1           /* 启动时显示Shell版本信息 */
#define SHELL_DEBUG_MODE                0           /* 启用调试模式(输出详细信息) */
/** @} */

/* ===========================================================================
 *                         历史记录功能配置区
 * ===========================================================================*/

#if SHELL_USING_HISTORY
/**
 * 历史记录配置
 * - 内存占用 = SHELL_HISTORY_MAX × SHELL_CMD_SIZE 字节
 * - 例如: 10 × 128 = 1280字节
 */
#define SHELL_HISTORY_MAX               10          /* 历史记录条数 */
#endif

/* ===========================================================================
 *                         权限管理功能配置区
 * ===========================================================================*/

#if SHELL_USING_AUTH

/**
 * 用户系统基础配置
 * - SHELL_MAX_USERS: 系统最大用户数
 * - SHELL_USERNAME_SIZE: 用户名最大长度(包含'\0')
 * - SHELL_PASSWORD_SIZE: 密码最大长度(包含'\0')
 */
#define SHELL_MAX_USERS                 8           /* 最大用户数 */
#define SHELL_USERNAME_SIZE             16          /* 用户名最大长度 */
#define SHELL_PASSWORD_SIZE             16          /* 密码最大长度 */

/**
 * 登录安全配置
 * - SHELL_MAX_LOGIN_TRIES: 最大登录失败次数，超过后可锁定
 * - SHELL_LOGIN_TIMEOUT: 登录超时时间(秒)，0表示无限制
 * - SHELL_AUTO_LOCK: 失败次数达到上限后是否自动锁定用户
 */
#define SHELL_MAX_LOGIN_TRIES           5           /* 最大登录尝试次数 */
#define SHELL_LOGIN_TIMEOUT             60          /* 登录超时时间(秒, 0=无限) */
#define SHELL_AUTO_LOCK_USER            0           /* 超限后自动锁定用户 */

/**
 * 密码安全配置
 * - SHELL_PASSWORD_ENCRYPT: 启用密码加密存储
 * - SHELL_PASSWORD_MIN_LEN: 密码最小长度要求
 */
#define SHELL_PASSWORD_ENCRYPT          0           /* 启用密码加密(需实现加密函数) */
#define SHELL_PASSWORD_MIN_LEN          4           /* 密码最小长度 */

/**
 * 默认管理员配置(编译时定义)
 * - 当未调用shell_user_init()时，系统使用此默认用户
 * - 建议在产品发布时修改默认密码
 */
#define SHELL_DEFAULT_USERNAME          "admin"         /* 默认管理员用户名 */
#define SHELL_DEFAULT_PASSWORD          "admin"         /* 默认管理员密码 */
#define SHELL_DEFAULT_AUTH              SHELL_AUTH_ADMIN /* 默认权限级别 */

#endif /* SHELL_USING_AUTH */

/* ===========================================================================
 *                      函数签名适配功能配置区
 * ===========================================================================*/

#if SHELL_USING_FUNC_SIGNATURE
/**
 * 参数适配配置
 * - 允许直接调用C函数，自动进行参数类型转换
 * - 内存占用: 约每个参数8字节(取决于平台)
 */
#define SHELL_PARAMETER_MAX_NUMBER      16          /* 函数最大参数数量 */
#endif

/* ===========================================================================
 *                           按键定义区
 * ===========================================================================*/

/**
 * 特殊按键ASCII码定义
 * - 兼容不同终端的按键编码
 * - 部分终端Backspace可能发送0x08或0x7F
 */
#define KEY_BACKSPACE                   0x08        /* 退格键(Backspace) */
#define KEY_DELETE                      0x7F        /* 删除键(Delete/0x7F) */
#define KEY_TAB                         0x09        /* Tab键(制表符) */
#define KEY_ENTER                       0x0D        /* 回车键(CR) */
#define KEY_NEWLINE                     0x0A        /* 换行键(LF) */
#define KEY_ESC                         0x1B        /* ESC键(转义) */
#define KEY_CTRL_C                      0x03        /* Ctrl+C(中断) */
#define KEY_CTRL_D                      0x04        /* Ctrl+D(退出) */
#define KEY_CTRL_L                      0x0C        /* Ctrl+L(清屏) */

/* ===========================================================================
 *                         版本信息配置区
 * ===========================================================================*/

#define SHELL_VERSION                   "2.0.0"     /* Shell版本号 */
#define SHELL_BUILD_DATE                __DATE__    /* 编译日期 */
#define SHELL_BUILD_TIME                __TIME__    /* 编译时间 */

/* ===========================================================================
 *                         兼容性检查区
 * ===========================================================================*/

/* 参数有效性检查 */
#if SHELL_CMD_SIZE < 16
    #error "SHELL_CMD_SIZE must be at least 16"
#endif

#if SHELL_ARG_MAX < 2
    #error "SHELL_ARG_MAX must be at least 2"
#endif

#if SHELL_USING_HISTORY && (SHELL_HISTORY_MAX < 1)
    #error "SHELL_HISTORY_MAX must be at least 1 when history is enabled"
#endif

#if SHELL_USING_AUTH && (SHELL_MAX_USERS < 1)
    #error "SHELL_MAX_USERS must be at least 1 when auth is enabled"
#endif

#endif /* __SHELL_CFG_H */
