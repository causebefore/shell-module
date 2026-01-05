/***
 * @Author: liu
 * @Date: 2025-12-09 18:39:37
 * @LastEditors: liu lbq08@foxmail.com
 * @LastEditTime: 2025-12-09 20:38:05
 * @FilePath: \shell\shell.h
 * @Description:
 * @
 * @Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

#ifndef __SHELL_H
#define __SHELL_H

#include "shell_cfg.h"

#include <stdint.h>

/* 用户权限级别 */
typedef enum
{
    SHELL_AUTH_GUEST = 0, /* 访客 - 只能执行基本命令 */
    SHELL_AUTH_USER  = 1, /* 普通用户 - 可执行大部分命令 */
    SHELL_AUTH_ADMIN = 2, /* 管理员 - 可执行所有命令 */
    SHELL_AUTH_ROOT  = 3  /* 超级管理员 - 最高权限 */
} shell_auth_level_t;
/* 用户信息结构 */
typedef struct
{
    const char*        username;    /* 用户名 */
    const char*        password;    /* 密码 */
    shell_auth_level_t auth_level;  /* 权限级别 */
    const char*        description; /* 用户描述 */
} shell_user_t;

/**
 * @brief 密码验证回调函数类型
 * @param username 用户名
 * @param password 用户输入的密码
 * @param stored_password 存储的密码（可能是加密的）
 * @return 0-验证成功, -1-验证失败
 */
typedef int (*shell_password_verify_t)(const char* username, const char* password, const char* stored_password);

typedef enum
{
    SHELL_TYPE_CMD_MAIN = 0, /**< main形式命令 */
    SHELL_TYPE_CMD_FUNC,     /**< C函数形式命令 */
} ShellCommandType;
/* 命令函数类型定义 */
typedef int (*shell_cmd_func_t)(int argc, char* argv[]);

/* 命令结构体 */

typedef struct
{
    const char* name;  /* 命令名称 */
    const char* desc;  /* 命令描述 */
    int (*function)(); /* 命令函数指针 */

    shell_auth_level_t auth_level; /* 执行此命令所需权限 */

    ShellCommandType type;     /* 命令类型: 0=普通命令, 1=变量命令, 2=参数适配模式命令 */
    unsigned char    paramNum; /* 参数适配模式下的参数数量 */
} shell_cmd_t;

/* Shell控制块 */
typedef struct
{
    const char* name;                       /* Shell实例名称 (如"UART1", "USB", "TCP") */
    char        cmd_buffer[SHELL_CMD_SIZE]; /* 命令缓冲区 */
    uint16_t    cmd_pos;                    /* 当前光标位置 */
    uint16_t    cmd_len;                    /* 当前命令长度 */

#if SHELL_USING_HISTORY
    char    history[SHELL_HISTORY_MAX][SHELL_CMD_SIZE]; /* 历史记录 */
    uint8_t history_count;                              /* 历史记录数量 */
    uint8_t history_index;                              /* 当前历史索引 */
    uint8_t history_cur;                                /* 当前浏览的历史 */
#endif

    const shell_cmd_t* cmd_list;  /* 命令列表 */
    uint16_t           cmd_count; /* 命令数量 */

    uint8_t esc_state;     /* ESC序列状态 */
    uint8_t esc_buffer[4]; /* ESC序列缓冲 */
    uint8_t esc_index;     /* ESC序列索引 */

#if SHELL_USING_AUTH
    const shell_user_t*     current_user;                         /* 当前登录用户 */
    char                    password_buffer[SHELL_PASSWORD_SIZE]; /* 密码输入缓冲 */
    uint8_t                 login_tries;                          /* 登录尝试次数 */
    uint8_t                 login_state;     /* 登录状态: 0=未登录, 1=输入用户名, 2=输入密码, 3=已登录 */
    shell_password_verify_t password_verify; /* 密码验证回调函数 */
#endif

    struct
    {
        unsigned char isChecked   : 1; /**< 密码校验通过 */
        unsigned char isActive    : 1; /**< 当前活动Shell */
        unsigned char passthrough : 1; /**< 透传模式 */
    } status;

    /* 透传回调函数 */
    void (*passthrough_handler)(uint8_t ch);

    /* 底层IO函数 */
    void (*write)(const char* data, uint16_t len);
    int (*read)(char* data, uint16_t len);
} shell_t;

/* Shell API */
void shell_init(shell_t* shell, const shell_cmd_t* cmd_list, uint16_t cmd_count);
void shell_init_ex(shell_t* shell, const char* name, const shell_cmd_t* cmd_list, uint16_t cmd_count,
                   void (*write_func)(const char* data, uint16_t len), int (*read_func)(char* data, uint16_t len));
void shell_set_io(shell_t* shell, void (*write_func)(const char* data, uint16_t len),
                  int (*read_func)(char* data, uint16_t len));
void shell_task(shell_t* shell);
void shell_log(const char* buffer, int len);
void shell_log_to(shell_t* shell, const char* buffer, int len); /* 定向输出日志 */
void shellHandler(shell_t* shell, char data);                   /* 中断安全的字符处理函数 */

/* 多Shell管理API */
void     shellAdd(shell_t* shell, const char* name);
void     shellRemove(shell_t* shell);
shell_t* shellGetCurrent(void);
shell_t* shellGetByName(const char* name);
int      shell_get_count(void);

/* 透传模式API */
void shell_passthrough_add_handler(shell_t* shell, void (*handler)(uint8_t ch));

#if SHELL_USING_AUTH
/* 用户权限管理API */
void                shell_user_init(const shell_user_t* users, uint16_t user_count);
int                 shell_login(shell_t* shell, const char* username, const char* password);
void                shell_logout(shell_t* shell);
const shell_user_t* shell_get_current_user(shell_t* shell);
shell_auth_level_t  shell_get_auth_level(shell_t* shell);
int                 shell_check_permission(shell_t* shell, shell_auth_level_t required_level);
const char*         shell_get_auth_name(shell_auth_level_t level);
void                shell_set_password_verify(shell_t* shell, shell_password_verify_t verify_func);
#endif

/* 辅助函数 */
void shell_print(shell_t* shell, const char* str);
void shell_printf(shell_t* shell, const char* format, ...);

/* 内置命令 */
int shell_cmd_help(int argc, char* argv[]);
int shell_cmd_clear(int argc, char* argv[]);
int shell_cmd_history(int argc, char* argv[]);
int shell_cmd_logout(int argc, char* argv[]);
int shell_cmd_echo(int argc, char* argv[]);
int shell_cmd_version(int argc, char* argv[]);
/* 内置命令列表获取 */
const shell_cmd_t* shell_get_builtin_commands(uint16_t* count);

/* 外部命令列表声明 */
extern const shell_cmd_t* shell_commands;
extern uint16_t           shell_cmd_count;

/* 命令列表初始化 */
void shell_commands_init(void);

/* 扩展命令执行(参数适配模式) */
int shellExtRun(shell_t* shell, shell_cmd_t* command, int argc, char* argv[]);

/* ===========================================================================
 *                          命令导出宏定义
 * ===========================================================================
 * 说明:
 * - 这些宏用于在代码中快速定义命令
 * - 根据是否启用权限管理,自动选择合适的宏
 * - 命令会被放入.shell_cmd段,由链接脚本收集
 * =========================================================================== */

// #if SHELL_USING_AUTH
///* 启用权限管理时的命令导出宏 */
// #def ine  SHELL_EXPORT_CMD(name, desc, func, auth)                                  \
//    const shell_cmd_t __shell_cmd_##func __attribute__((section(".shell_cmd"))) = \
//        {name, desc, func, auth, SHELL_TYPE_CMD_MAIN, 0}

//#define SHELL_EXPORT_CMD_EX(name, desc, func, auth, type, paramNum)               \
//    const shell_cmd_t __shell_cmd_##func __attribute__((section(".shell_cmd"))) = \
//        {name, desc, func, auth, type, paramNum}

// #else
///* 未启用权限管理时的命令导出宏(权限默认为GUEST) */
// #def ine  SHELL_EXPORT_CMD(name, desc, func)                                        \
//    const shell_cmd_t __shell_cmd_##func __attribute__((section(".shell_cmd"))) = \
//        {name, desc, func, SHELL_AUTH_GUEST, SHELL_TYPE_CMD_MAIN, 0}

//#define SHELL_EXPORT_CMD_EX(name, desc, func, type, paramNum)                     \
//    const shell_cmd_t __shell_cmd_##func __attribute__((section(".shell_cmd"))) = \
//        {name, desc, func, SHELL_AUTH_GUEST, type, paramNum}

// #endif /* SHELL_USING_AUTH */

#endif /* __SHELL_H */
