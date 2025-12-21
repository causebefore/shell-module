/*****************************************************************************
 * @file        shell.c
 * @brief       Shell命令行系统核心实现
 * @author      liu
 * @date        2025-12-09
 * @version     2.0
 * @copyright   Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 *
 * @details     本文件实现Shell系统的核心功能，包括：
 *              - 命令解析和执行
 *              - 历史记录管理
 *              - 命令补全
 *              - 用户权限管理
 *              - 透传模式
 *              - 多Shell实例管理
 *              - 日志输出功能
 *
 * @note        功能裁剪通过shell_cfg.h中的宏定义控制
 *****************************************************************************/

#include "shell.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* ===========================================================================
 *                       多Shell实例管理(全局变量)
 * ===========================================================================*/

#if SHELL_USING_MULTI_INSTANCE
/**
 * @brief Shell实例管理数组
 * @note 支持同时管理多个Shell实例(UART/USB/TCP等)
 */
static shell_t *g_shell_instances[SHELL_MAX_INSTANCES] = {NULL};
static int g_shell_count = 0; /**< 当前已注册的Shell实例数量 */
#else
/**
 * @brief 单Shell实例指针
 * @note 禁用多实例时，只支持一个Shell实例
 */
static shell_t *g_current_shell = NULL;
#endif

/* ===========================================================================
 *                       用户权限管理(全局变量)
 * ===========================================================================*/

#if SHELL_USING_AUTH

/**
 * @brief 全局用户列表指针
 * @note 指向ROM中的用户数组，由shell_user_init()设置
 */
static const shell_user_t *g_users = NULL;
static uint16_t g_user_count = 0;

/**
 * @brief 默认管理员用户(编译时定义)
 * @note 当未调用shell_user_init()时使用此用户
 */
static const shell_user_t g_default_user = {
    .username = SHELL_DEFAULT_USERNAME,
    .password = SHELL_DEFAULT_PASSWORD,
    .auth_level = SHELL_DEFAULT_AUTH,
    .description = "Default administrator account"};

#endif /* SHELL_USING_AUTH */

/* ===========================================================================
 *                          ANSI控制序列定义
 * ===========================================================================*/

#define ANSI_CLEAR_LINE "\033[2K\r"       /**< 清除当前行 */
#define ANSI_CLEAR_SCREEN "\033[2J\033[H" /**< 清屏并移动到左上角 */
#define ANSI_MOVE_LEFT "\033[1D"          /**< 光标左移一格 */
#define ANSI_MOVE_RIGHT "\033[1C"         /**< 光标右移一格 */

/* ===========================================================================
 *                          基础输出函数
 * ===========================================================================*/

/**
 * @brief 输出字符串到Shell
 * @param shell Shell实例指针
 * @param str 字符串指针
 */
void shell_print(shell_t *shell, const char *str)
{
    if (shell != NULL && shell->write != NULL)
    {
        shell->write(str, strlen(str));
    }
}

/**
 * @brief 格式化输出到Shell(类似printf)
 * @param shell Shell实例指针
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void shell_printf(shell_t *shell, const char *format, ...)
{
    char buffer[256];
    va_list args;

    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0 && shell != NULL && shell->write != NULL)
    {
        shell->write(buffer, len);
    }
}

/**
 * @brief 显示Shell提示符
 * @param shell Shell实例指针
 * @note 根据配置显示不同格式的提示符
 */
static void shell_show_prompt(shell_t *shell)
{
#if SHELL_USING_AUTH
    /* 启用权限管理：显示用户名@实例名> */
    if (shell->current_user != NULL)
    {
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "%s@%s> ",
                 shell->current_user->username,
                 shell->name ? shell->name : "shell");
        shell_print(shell, prompt);
    }
    else
    {
        /* 未登录状态：显示登录提示 */
        shell_print(shell, "login: ");
    }
#else
    /* 未启用权限：显示默认提示符 */
    shell_print(shell, SHELL_PROMPT);
#endif
}

/* ===========================================================================
 *                          透传模式实现
 * ===========================================================================*/

#if SHELL_USING_PASSTHROUGH

/**
 * @brief 默认透传数据处理函数(回显)
 * @param ch 接收到的字符
 *
 * @note 用户可自定义此函数，实现特定的透传处理逻辑
 */
void shell_passthrough_add_handler(shell_t *shell, void (*handler)(uint8_t ch))
{
    if (shell == NULL)
    {
        return;
    }
    if (handler == NULL)
    {
        shell_printf(shell, "\r\n[Error: Passthrough handler is NULL]\r\n");
        return;
    }

    shell->passthrough_handler = handler;

    shell_printf(shell, "\r\n[Passthrough mode ON. Press Ctrl+] to exit]\r\n");
}

/**
 * @brief 进入透传模式
 * @param shell Shell实例指针
 * @param handler 透传数据处理回调函数
 *
 * @note 透传模式下，所有接收到的数据直接转发给handler处理
 *       不再进行命令解析，按Ctrl+]退出透传模式
 *
 * @usage 典型应用场景：
 *        - AT指令透传(4G/WiFi模块)
 *        - 串口转发
 *        - 原始数据传输
 */
void shell_cmd_enter_passthrough(shell_t *shell)
{
    if (shell == NULL)
    {
        return;
    }
    if (shell->passthrough_handler == NULL)
    {
        shell_printf(shell, "\r\n[Error: Passthrough handler is NULL]\r\n");
        return;
    }

    shell->status.passthrough = 1;

    shell_printf(shell, "\r\n[Passthrough mode ON. Press Ctrl+] to exit]\r\n");
}

/**
 * @brief 退出透传模式
 * @param shell Shell实例指针
 */
void shell_cmd_exit_passthrough(shell_t *shell)
{
    if (shell == NULL)
    {
        return;
    }

    shell->status.passthrough = 0;
    shell->passthrough_handler = NULL;

    shell_printf(shell, "\r\n[Passthrough mode OFF]\r\n");
    shell_show_prompt(shell);
}

/**
 * @brief 检查是否处于透传模式
 * @param shell Shell实例指针
 * @return 1=透传模式，0=正常模式
 */
uint8_t shell_cmd_is_passthrough(shell_t *shell)
{
    if (shell == NULL)
    {
        return 0;
    }
    return shell->status.passthrough;
}

#endif /* SHELL_USING_PASSTHROUGH */

/* ===========================================================================
 *                          Shell初始化和管理
 * ===========================================================================*/

/**
 * @brief 初始化Shell实例(基础版本)
 * @param shell Shell实例指针
 * @param cmd_list 命令列表指针(ROM)
 * @param cmd_count 命令数量
 *
 * @note 这是基础初始化函数，只设置命令列表
 *       推荐使用 shell_init_ex() 进行完整初始化
 */
void shell_init(shell_t *shell, const shell_cmd_t *cmd_list, uint16_t cmd_count)
{
    /* 清零Shell结构体 */
    memset(shell, 0, sizeof(shell_t));

    /* 设置命令列表 */
    shell->cmd_list = cmd_list;
    shell->cmd_count = cmd_count;

    /* 设置默认名称 */
    shell->name = "default";
}

/**
 * @brief 初始化Shell实例(扩展版本 - 一步到位)
 * @param shell Shell实例指针
 * @param name Shell名称(用于标识)
 * @param cmd_list 命令列表指针(ROM)
 * @param cmd_count 命令数量
 * @param write_func 写函数指针
 * @param read_func 读函数指针
 *
 * @note 推荐使用此函数，一次调用完成所有初始化
 *       等同于依次调用：
 *       - shell_init()
 *       - shell_set_io()
 *       - shellAdd()
 *
 * @example
 *   // 获取IO函数
 *   void (*write)(const char*, uint16_t) = shell_get_write_function();
 *   int (*read)(char*, uint16_t) = shell_get_read_function();
 *
 *   // 一步初始化
 *   shell_init_ex(&my_shell, "UART1", shell_commands, shell_cmd_count,
 *                 write, read);
 */
void shell_init_ex(shell_t *shell,
                   const char *name,
                   const shell_cmd_t *cmd_list,
                   uint16_t cmd_count,
                   void (*write_func)(const char *data, uint16_t len),
                   int (*read_func)(char *data, uint16_t len))
{
    if (shell == NULL)
    {
        return;
    }

    /* 基础初始化 */
    shell_init(shell, cmd_list, cmd_count);

    /* 设置名称 */
    if (name != NULL)
    {
        shell->name = name;
    }

    /* 设置IO函数 */
    shell->write = write_func;
    shell->read = read_func;

#if SHELL_USING_MULTI_INSTANCE
    /* 自动注册到管理器 */
    shellAdd(shell, name);
#else
    /* 单实例模式：直接保存指针 */
    g_current_shell = shell;
#endif
}

/**
 * @brief 设置Shell的IO函数
 * @param shell Shell实例指针
 * @param write_func 写函数指针
 * @param read_func 读函数指针
 */
void shell_set_io(shell_t *shell,
                  void (*write_func)(const char *data, uint16_t len),
                  int (*read_func)(char *data, uint16_t len))
{
    if (shell != NULL)
    {
        shell->write = write_func;
        shell->read = read_func;
    }
}

#if SHELL_USING_MULTI_INSTANCE
/**
 * @brief 注册Shell实例到全局管理器
 * @param shell Shell实例指针
 * @param name Shell名称(用于标识)
 *
 * @note 多Shell实例管理，便于同时使用UART/USB/TCP等多个终端
 */
void shellAdd(shell_t *shell, const char *name)
{
    if (shell == NULL)
    {
        return;
    }

    for (int i = 0; i < SHELL_MAX_INSTANCES; i++)
    {
        if (g_shell_instances[i] == NULL)
        {
            shell->name = name;
            g_shell_instances[i] = shell;
            g_shell_count++;
            return;
        }
    }
}

/**
 * @brief 从管理器中移除Shell实例
 * @param shell Shell实例指针
 */
void shellRemove(shell_t *shell)
{
    if (shell == NULL)
    {
        return;
    }

    for (int i = 0; i < SHELL_MAX_INSTANCES; i++)
    {
        if (g_shell_instances[i] == shell)
        {
            /* 移动后续实例 */
            for (int j = i; j < SHELL_MAX_INSTANCES - 1; j++)
            {
                g_shell_instances[j] = g_shell_instances[j + 1];
            }
            g_shell_instances[SHELL_MAX_INSTANCES - 1] = NULL;
            g_shell_count--;
            return;
        }
    }
}

/**
 * @brief 获取当前激活的Shell实例
 * @return Shell实例指针，无激活实例返回NULL
 *
 * @note 激活状态由isActive标志决定，在处理命令时自动设置
 */
shell_t *shellGetCurrent(void)
{
    for (int i = 0; i < SHELL_MAX_INSTANCES; i++)
    {
        if (g_shell_instances[i] != NULL &&
            g_shell_instances[i]->status.isActive)
        {
            return g_shell_instances[i];
        }
    }
    return NULL;
}

/**
 * @brief 根据名称获取Shell实例
 * @param name Shell名称
 * @return Shell实例指针，未找到返回NULL
 */
shell_t *shellGetByName(const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }

    for (int i = 0; i < SHELL_MAX_INSTANCES; i++)
    {
        if (g_shell_instances[i] != NULL &&
            g_shell_instances[i]->name != NULL &&
            strcmp(g_shell_instances[i]->name, name) == 0)
        {
            return g_shell_instances[i];
        }
    }

    return NULL;
}

/**
 * @brief 获取已注册的Shell实例数量
 * @return 实例数量
 */
int shell_get_count(void)
{
    return g_shell_count;
}

#else /* !SHELL_USING_MULTI_INSTANCE */

/**
 * @brief 注册Shell实例(单实例模式)
 * @param shell Shell实例指针
 * @param name Shell名称(忽略)
 */
void shellAdd(shell_t *shell, const char *name)
{
    (void)name;
    g_current_shell = shell;
}

/**
 * @brief 移除Shell实例(单实例模式)
 * @param shell Shell实例指针
 */
void shellRemove(shell_t *shell)
{
    if (g_current_shell == shell)
    {
        g_current_shell = NULL;
    }
}

/**
 * @brief 获取当前Shell实例(单实例模式)
 * @return Shell实例指针
 */
shell_t *shellGetCurrent(void)
{
    return g_current_shell;
}

/**
 * @brief 根据名称获取Shell实例(单实例模式，忽略名称)
 * @param name Shell名称(忽略)
 * @return Shell实例指针
 */
shell_t *shellGetByName(const char *name)
{
    (void)name;
    return g_current_shell;
}

/**
 * @brief 获取Shell实例数量(单实例模式)
 * @return 0或1
 */
int shell_get_count(void)
{
    return (g_current_shell != NULL) ? 1 : 0;
}

#endif /* SHELL_USING_MULTI_INSTANCE */

#if SHELL_USING_AUTH
/* ==================== 用户权限管理 ==================== */

/**
 * @brief 默认密码验证函数（简单字符串比较）
 * @param username 用户名（可用于日志）
 * @param password 用户输入的密码
 * @param stored_password 存储的密码
 * @return 0-验证成功, -1-验证失败
 */
static int shell_default_password_verify(const char *username, const char *password, const char *stored_password)
{
    (void)username; /* 未使用的参数 */
    return strcmp(password, stored_password);
}

/**
 * @brief 设置密码验证回调函数
 * @param shell Shell实例
 * @param verify_func 密码验证函数，为NULL则使用默认验证
 */
void shell_set_password_verify(shell_t *shell, shell_password_verify_t verify_func)
{
    if (shell == NULL)
    {
        return;
    }

    shell->password_verify = verify_func ? verify_func : shell_default_password_verify;
}

/**
 * @brief 初始化用户系统
 * @param users 用户列表 (为NULL时仅使用默认用户)
 * @param user_count 用户数量
 *
 * @note 默认admin用户始终存在，自定义用户列表会追加到默认用户之后
 *       这样可以确保始终有一个后备管理员账户
 */
void shell_user_init(const shell_user_t *users, uint16_t user_count)
{
    /* 默认用户始终指向g_default_user，确保有后备账户 */
    g_users = &g_default_user;
    g_user_count = 1;

    /* 如果提供了自定义用户列表，则使用自定义列表 */
    if (users != NULL && user_count > 0)
    {
        g_users = users;
        g_user_count = user_count;
    }
}

/**
 * @brief 用户登录
 * @param shell Shell实例
 * @param username 用户名
 * @param password 密码
 * @return 0-成功, -1-失败
 */
int shell_login(shell_t *shell, const char *username, const char *password)
{
    if (shell == NULL || username == NULL || password == NULL)
    {
        return -1;
    }

    /* 如果未初始化用户系统，自动使用默认用户 */
    if (g_users == NULL || g_user_count == 0)
    {
        g_users = &g_default_user;
        g_user_count = 1;
    }

    /* 如果未设置密码验证回调，使用默认验证 */
    if (shell->password_verify == NULL)
    {
        shell->password_verify = shell_default_password_verify;
    }

    /* 先查找自定义用户列表 */
    for (uint16_t i = 0; i < g_user_count; i++)
    {
        if (strcmp(g_users[i].username, username) == 0)
        {
            /* 使用回调函数验证密码 */
            if (shell->password_verify(username, password, g_users[i].password) == 0)
            {
                shell->current_user = &g_users[i];
                shell->login_state = 3; /* 已登录 */
                shell->login_tries = 0;
                return 0;
            }
            else
            {
                /* 密码错误 */
                shell->login_tries++;
                return -1;
            }
        }
    }

    /* 用户列表中未找到，尝试使用默认用户作为后备 */
    if (g_users != &g_default_user)
    {
        if (strcmp(g_default_user.username, username) == 0)
        {
            if (shell->password_verify(username, password, g_default_user.password) == 0)
            {
                shell->current_user = &g_default_user;
                shell->login_state = 3;
                shell->login_tries = 0;
                return 0;
            }
        }
    }

    /* 用户名不存在或密码错误 */
    shell->login_tries++;
    return -1;
}

/**
 * @brief 用户登出
 * @param shell Shell实例
 */
void shell_logout(shell_t *shell)
{
    if (shell == NULL)
    {
        return;
    }

    shell->current_user = NULL;
    shell->login_state = 0; /* 返回未登录状态 */
    shell->login_tries = 0;
    memset(shell->password_buffer, 0, sizeof(shell->password_buffer));

    /* 清空命令缓冲区 */
    shell->cmd_len = 0;
    shell->cmd_pos = 0;
    shell->cmd_buffer[0] = '\0';
}

/**
 * @brief 获取当前登录用户
 * @param shell Shell实例
 * @return 用户指针，未登录返回NULL
 */
const shell_user_t *shell_get_current_user(shell_t *shell)
{
    if (shell == NULL)
    {
        return NULL;
    }
    return shell->current_user;
}

/**
 * @brief 获取当前用户权限级别
 * @param shell Shell实例
 * @return 权限级别
 */
shell_auth_level_t shell_get_auth_level(shell_t *shell)
{
    if (shell == NULL || shell->current_user == NULL)
    {
        return SHELL_AUTH_GUEST;
    }
    return shell->current_user->auth_level;
}

/**
 * @brief 检查权限
 * @param shell Shell实例
 * @param required_level 所需权限级别
 * @return 1-有权限, 0-无权限
 */
int shell_check_permission(shell_t *shell, shell_auth_level_t required_level)
{
    if (shell == NULL)
    {
        return 0;
    }

    shell_auth_level_t current_level = shell_get_auth_level(shell);
    return (current_level >= required_level) ? 1 : 0;
}

/**
 * @brief 获取权限级别名称
 * @param level 权限级别
 * @return 权限名称字符串
 */
const char *shell_get_auth_name(shell_auth_level_t level)
{
    switch (level)
    {
    case SHELL_AUTH_GUEST:
        return "Guest";
    case SHELL_AUTH_USER:
        return "User";
    case SHELL_AUTH_ADMIN:
        return "Admin";
    case SHELL_AUTH_ROOT:
        return "Root";
    default:
        return "Unknown";
    }
}

#endif /* SHELL_USING_AUTH */

/**
 * @brief 刷新命令行显示
 */
static void shell_refresh_line(shell_t *shell)
{
    /* 清除当前行 */
    shell_print(shell, ANSI_CLEAR_LINE);

    /* 显示提示符和命令 */
    shell_show_prompt(shell);
    shell->write(shell->cmd_buffer, shell->cmd_len);

    /* 移动光标到正确位置 */
    int move_count = shell->cmd_len - shell->cmd_pos;
    for (int i = 0; i < move_count; i++)
    {
        shell_print(shell, ANSI_MOVE_LEFT);
    }
}

#if SHELL_USING_HISTORY
/**
 * @brief 添加到历史记录
 */
static void shell_add_history(shell_t *shell, const char *cmd)
{
    if (cmd == NULL || cmd[0] == '\0')
    {
        return;
    }

    /* 避免重复记录 */
    if (shell->history_count > 0)
    {
        uint8_t last = (shell->history_index == 0) ? (SHELL_HISTORY_MAX - 1) : (shell->history_index - 1);
        if (strcmp(shell->history[last], cmd) == 0)
        {
            return;
        }
    }

    /* 添加新记录 */
    strncpy(shell->history[shell->history_index], cmd, SHELL_CMD_SIZE - 1);
    shell->history[shell->history_index][SHELL_CMD_SIZE - 1] = '\0';

    shell->history_index = (shell->history_index + 1) % SHELL_HISTORY_MAX;
    if (shell->history_count < SHELL_HISTORY_MAX)
    {
        shell->history_count++;
    }
}

/**
 * @brief 获取历史记录
 */
static const char *shell_get_history(shell_t *shell, int direction)
{
    if (shell->history_count == 0)
    {
        return NULL;
    }

    if (direction > 0)
    { /* 上键 */
        shell->history_cur = (shell->history_cur + SHELL_HISTORY_MAX - 1) % SHELL_HISTORY_MAX;
    }
    else
    { /* 下键 */
        shell->history_cur = (shell->history_cur + 1) % SHELL_HISTORY_MAX;
    }

    return shell->history[shell->history_cur];
}
#endif

#if SHELL_USING_COMPLETION
/**
 * @brief 显示命令列表(Tab补全和help命令共用)
 * @param shell Shell实例
 * @param prefix 命令前缀(为NULL则显示所有命令)
 * @param show_tips 是否显示使用提示
 */
static void shell_show_commands(shell_t *shell, const char *prefix, uint8_t show_tips)
{
    uint16_t prefix_len = (prefix != NULL) ? strlen(prefix) : 0;
    uint16_t match_count = 0;

    /* 统计匹配数量 */
    for (uint16_t i = 0; i < shell->cmd_count; i++)
    {
        if (prefix == NULL || strncmp(shell->cmd_list[i].name, prefix, prefix_len) == 0)
        {
            match_count++;
        }
    }

    if (match_count == 0)
    {
        return;
    }

    /* 显示表头 */
    shell_print(shell, "\r\n");
    if (prefix == NULL && show_tips)
    {
        shell_print(shell, "Available commands:\r\n");
    }

#if SHELL_USING_AUTH
    shell_print(shell, "----------------------------------------\r\n");
    shell_printf(shell, "  %-16s      %-8s       %s\r\n", "Command", "Auth", "Description");
    shell_print(shell, "----------------------------------------\r\n");

    for (uint16_t i = 0; i < shell->cmd_count; i++)
    {
        if (prefix == NULL || strncmp(shell->cmd_list[i].name, prefix, prefix_len) == 0)
        {
            const char *auth_str = shell_get_auth_name(shell->cmd_list[i].auth_level);
            shell_printf(shell, "  %-16s   %-8s   %s\r\n",
                         shell->cmd_list[i].name,
                         auth_str,
                         shell->cmd_list[i].desc);
        }
    }
#else
    shell_print(shell, "----------------------------------------\r\n");

    for (uint16_t i = 0; i < shell->cmd_count; i++)
    {
        if (prefix == NULL || strncmp(shell->cmd_list[i].name, prefix, prefix_len) == 0)
        {
            shell_printf(shell, "  %-16s - %s\r\n",
                         shell->cmd_list[i].name,
                         shell->cmd_list[i].desc);
        }
    }
#endif

    shell_print(shell, "----------------------------------------\r\n");

    /* 显示额外信息 */
    if (show_tips)
    {
#if SHELL_USING_AUTH
        shell_print(shell, "\r\nCurrent user: ");
        if (shell->current_user != NULL)
        {
            shell_printf(shell, "%s (%s)\r\n",
                         shell->current_user->username,
                         shell_get_auth_name(shell->current_user->auth_level));
        }
        else
        {
            shell_print(shell, "Not logged in\r\n");
        }
#endif
        shell_print(shell, "\r\nTips:\r\n");
        shell_print(shell, "  - Press TAB for command completion\r\n");
        shell_print(shell, "  - Use UP/DOWN keys for history\r\n");
        shell_print(shell, "  - Use LEFT/RIGHT keys to move cursor\r\n");
    }

    shell_print(shell, "\r\n");
}

/**
 * @brief 命令补全
 */
static void shell_completion(shell_t *shell)
{
    const char *prefix = shell->cmd_buffer;
    uint16_t prefix_len = shell->cmd_len;
    const shell_cmd_t *match = NULL;
    uint16_t match_count = 0;

    /* 查找匹配的命令 */
    for (uint16_t i = 0; i < shell->cmd_count; i++)
    {
        if (strncmp(shell->cmd_list[i].name, prefix, prefix_len) == 0)
        {
            match = &shell->cmd_list[i];
            match_count++;
        }
    }

    if (match_count == 1)
    {
        /* 唯一匹配，自动补全 */
        strncpy(shell->cmd_buffer, match->name, SHELL_CMD_SIZE - 1);
        shell->cmd_buffer[SHELL_CMD_SIZE - 1] = '\0';
        shell->cmd_len = strlen(shell->cmd_buffer);
        shell->cmd_pos = shell->cmd_len;
        shell_refresh_line(shell);
    }
    else if (match_count > 1)
    {
        /* 多个匹配，显示匹配的命令列表 */
        shell_show_commands(shell, prefix, 0);
        shell_refresh_line(shell);
    }
}
#endif

/**
 * @brief 解析并执行命令
 */
static void shell_exec_cmd(shell_t *shell)
{
    char *argv[SHELL_ARG_MAX];
    int argc = 0;
    char *p = shell->cmd_buffer;

    /* 分割参数 */
    while (*p != '\0' && argc < SHELL_ARG_MAX)
    {
        /* 跳过空格 */
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }

        if (*p == '\0')
        {
            break;
        }

        /* 记录参数 */
        argv[argc++] = p;

        /* 查找参数结束 */
        while (*p != '\0' && *p != ' ' && *p != '\t')
        {
            p++;
        }

        if (*p != '\0')
        {
            *p++ = '\0';
        }
    }

    if (argc == 0)
    {
        return;
    }

    /* 查找并执行命令 */
    for (uint16_t i = 0; i < shell->cmd_count; i++)
    {
        if (strcmp(shell->cmd_list[i].name, argv[0]) == 0)
        {
#if SHELL_USING_AUTH
            /* 检查权限 */
            if (!shell_check_permission(shell, shell->cmd_list[i].auth_level))
            {
                shell_printf(shell, "Permission denied. Required: %s, Current: %s\r\n",
                             shell_get_auth_name(shell->cmd_list[i].auth_level),
                             shell_get_auth_name(shell_get_auth_level(shell)));
                return;
            }
#endif
            /* 执行命令 - 统一使用标准签名 */
            int ret = shell->cmd_list[i].function(argc, argv);

            if (ret != 0)
            {
                shell_printf(shell, "Command returned error: %d\r\n", ret);
            }
            return;
        }
    }

    shell_printf(shell, "Unknown command: %s\r\n", argv[0]);
    shell_printf(shell, "Type 'help' for available commands\r\n");
}

/**
 * @brief 处理ESC序列(方向键等)
 */
static void shell_handle_esc(shell_t *shell, char ch)
{
    shell->esc_buffer[shell->esc_index++] = ch;

    /* 检查是否是完整的ESC序列 (ESC [ X 共3字节，但esc_buffer只存后2字节) */
    if (shell->esc_index >= 2 && shell->esc_buffer[0] == '[')
    {
        char code = shell->esc_buffer[1];

        switch (code)
        {
#if SHELL_USING_HISTORY
        case 'A': /* 上键 */
        {
            const char *hist = shell_get_history(shell, 1);
            if (hist)
            {
                strncpy(shell->cmd_buffer, hist, SHELL_CMD_SIZE - 1);
                shell->cmd_buffer[SHELL_CMD_SIZE - 1] = '\0';
                shell->cmd_len = strlen(shell->cmd_buffer);
                shell->cmd_pos = shell->cmd_len;
                shell_refresh_line(shell);
            }
        }
        break;

        case 'B': /* 下键 */
        {
            const char *hist = shell_get_history(shell, -1);
            if (hist)
            {
                strncpy(shell->cmd_buffer, hist, SHELL_CMD_SIZE - 1);
                shell->cmd_buffer[SHELL_CMD_SIZE - 1] = '\0';
                shell->cmd_len = strlen(shell->cmd_buffer);
                shell->cmd_pos = shell->cmd_len;
                shell_refresh_line(shell);
            }
        }
        break;
#endif
        case 'C': /* 右键 */
            if (shell->cmd_pos < shell->cmd_len)
            {
                shell->cmd_pos++;
                shell_print(shell, ANSI_MOVE_RIGHT);
            }
            break;

        case 'D': /* 左键 */
            if (shell->cmd_pos > 0)
            {
                shell->cmd_pos--;
                shell_print(shell, ANSI_MOVE_LEFT);
            }
            break;
        }

        shell->esc_state = 0;
        shell->esc_index = 0;
    }

    /* 序列太长，重置 */
    if (shell->esc_index >= sizeof(shell->esc_buffer))
    {
        shell->esc_state = 0;
        shell->esc_index = 0;
    }
}

/**
 * @brief 处理单个字符输入
 */
void shell_handle_char(shell_t *shell, char ch)
{
#if SHELL_USING_AUTH
    /* 登录流程处理 */
    if (shell->login_state < 3)
    {
        /* login_state: 0=输入用户名, 1=输入密码, 2=验证中, 3=已登录 */

        if (ch == KEY_ENTER || ch == KEY_NEWLINE)
        {
            shell_print(shell, "\r\n");

            if (shell->login_state == 0)
            {
                /* 用户名输入完成 */
                shell->cmd_buffer[shell->cmd_len] = '\0';
                shell->login_state = 1;
                shell_print(shell, "Password: ");
                /* 清空缓冲区准备输入密码 */
                shell->cmd_len = 0;
                shell->cmd_pos = 0;
            }
            else if (shell->login_state == 1)
            {
                /* 密码输入完成，验证 */
                shell->password_buffer[shell->cmd_len] = '\0';
                char username[SHELL_USERNAME_SIZE];
                strncpy(username, shell->cmd_buffer, sizeof(username));

                if (shell_login(shell, username, shell->password_buffer) == 0)
                {
                    /* 清屏 */
                    shell_print(shell, ANSI_CLEAR_SCREEN);

                    shell_print(shell, "Login successful!\r\n");
                    const shell_user_t *user = shell_get_current_user(shell);
                    if (user != NULL)
                    {
                        shell_printf(shell, "Welcome, %s (%s)\r\n\r\n",
                                     user->username,
                                     shell_get_auth_name(user->auth_level));
                    }
                    shell->login_state = 3; /* 已登录 */
                }
                else
                {
                    shell_print(shell, "\r\nLogin failed!\r\n");
                    shell_printf(shell, "Attempts: %d/%d\r\n\r\n",
                                 shell->login_tries, SHELL_MAX_LOGIN_TRIES);

                    if (shell->login_tries >= SHELL_MAX_LOGIN_TRIES)
                    {
                        shell_print(shell, "Too many failed attempts!\r\n");
                        shell_print(shell, "System locked. Please reset.\r\n");
                        return;
                    }

                    /* 重新输入用户名 */
                    shell->login_state = 0;
                }

                /* 清空密码缓冲 */
                memset(shell->password_buffer, 0, sizeof(shell->password_buffer));
                shell->cmd_len = 0;
                shell->cmd_pos = 0;
                shell->cmd_buffer[0] = '\0';
                shell_show_prompt(shell);
            }
            return;
        }
        else if (ch == KEY_BACKSPACE || ch == KEY_DELETE)
        {
            /* 删除字符 */
            if (shell->cmd_pos > 0)
            {
                shell->cmd_pos--;
                shell->cmd_len--;
                if (shell->login_state == 0)
                {
                    /* 用户名显示删除 */
                    shell->cmd_buffer[shell->cmd_len] = '\0';
                    shell_print(shell, "\b \b");
                }
                else if (shell->login_state == 1)
                {
                    /* 密码删除 - 同时清除缓冲区 */
                    shell->password_buffer[shell->cmd_len] = '\0';
                    shell_print(shell, "\b \b");
                }
            }
            return;
        }
        else if (ch >= 0x20 && ch < 0x7F)
        {
            /* 可打印字符 */
            if (shell->cmd_len < SHELL_CMD_SIZE - 1)
            {
                if (shell->login_state == 0)
                {
                    /* 用户名 - 显示 */
                    shell->cmd_buffer[shell->cmd_len] = ch;
                    shell->cmd_len++;
                    shell->cmd_pos++;
                    shell->write(&ch, 1);
                }
                else if (shell->login_state == 1)
                {
                    /* 密码 - 不显示，用*代替 */
                    shell->password_buffer[shell->cmd_len] = ch;
                    shell->cmd_len++;
                    shell->cmd_pos++;
                    shell_print(shell, "*");
                }
            }
            return;
        }
        return; /* 登录状态下忽略其他按键 */
    }
#endif

    /* 透传模式处理 */
#if SHELL_USING_PASSTHROUGH
    if (shell->status.passthrough == 1)
    {
        /* Ctrl+] (ASCII 29) 退出Passthrough模式 */
        if (ch == 29)
        {
            shell_cmd_exit_passthrough(shell);
            return;
        }

        /* 调用透传处理函数 */
        if (shell->passthrough_handler != NULL)
        {
            shell->passthrough_handler((uint8_t)ch);
        }
        return;
    }
#endif
    /* 处理ESC序列 */
    if (shell->esc_state)
    {
        shell_handle_esc(shell, ch);
        return;
    }

    switch (ch)
    {
    case KEY_ESC:
        shell->esc_state = 1;
        shell->esc_index = 0;
        break;

    case KEY_TAB:
#if SHELL_USING_COMPLETION
        shell_completion(shell);
#endif
        break;

    case KEY_BACKSPACE: /* 0x08 */
    case KEY_DELETE:    /* 0x7F */
        /* 兼容不同终端的Backspace键值 (0x08或0x7F) */
        if (shell->cmd_pos > 0)
        {
            /* 删除光标前的字符 */
            memmove(&shell->cmd_buffer[shell->cmd_pos - 1],
                    &shell->cmd_buffer[shell->cmd_pos],
                    shell->cmd_len - shell->cmd_pos);
            shell->cmd_pos--;
            shell->cmd_len--;
            shell->cmd_buffer[shell->cmd_len] = '\0';
            shell_refresh_line(shell);
        }
        break;

    case KEY_ENTER:
    case KEY_NEWLINE:
        shell_print(shell, "\r\n");
        shell->cmd_buffer[shell->cmd_len] = '\0';

#if SHELL_USING_HISTORY
        shell_add_history(shell, shell->cmd_buffer);
        shell->history_cur = shell->history_index;
#endif

        shell_exec_cmd(shell);

        /* 重置命令缓冲 */
        shell->cmd_len = 0;
        shell->cmd_pos = 0;
        shell->cmd_buffer[0] = '\0';

        shell_show_prompt(shell);
        break;

    default:
        /* 可打印字符 */
        if (ch >= 0x20 && ch < 0x7F)
        {
            if (shell->cmd_len < SHELL_CMD_SIZE - 1)
            {
                /* 在光标位置插入字符 */
                if (shell->cmd_pos < shell->cmd_len)
                {
                    memmove(&shell->cmd_buffer[shell->cmd_pos + 1],
                            &shell->cmd_buffer[shell->cmd_pos],
                            shell->cmd_len - shell->cmd_pos);
                }
                shell->cmd_buffer[shell->cmd_pos] = ch;
                shell->cmd_pos++;
                shell->cmd_len++;
                shell->cmd_buffer[shell->cmd_len] = '\0';
                shell_refresh_line(shell);
            }
        }
        break;
    }
}

/**
 * @brief Shell任务(主循环)
 */
void shell_task(shell_t *shell)
{
    static uint8_t first_run = 1;

    if (first_run)
    {
        first_run = 0;
			  shell_print(shell, ANSI_CLEAR_SCREEN);
        shell_print(shell, "\r\n");
        shell_print(shell, "========================================\r\n");
        shell_print(shell, "  Embedded Shell v1.0\r\n");
#if SHELL_USING_AUTH
        shell_print(shell, "  User Authentication: Enabled\r\n");
#endif
        shell_print(shell, "  Type 'help' for available commands\r\n");
        shell_print(shell, "  version: 2025-12-09\r\n");
        shell_print(shell, "  Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.\r\n");
        shell_print(shell, "========================================\r\n");
        shell_print(shell, "\r\n");

#if SHELL_USING_AUTH
        /* 启用权限时，要求先登录 */
        shell->login_state = 0; /* 0=未登录 */
        shell_print(shell, "Please login to continue.\r\n");
#endif
        shell_show_prompt(shell);
    }

    /* 读取并处理输入 */
    if (shell->read)
    {
        char ch;
        if (shell->read(&ch, 1) > 0)
        {
            shell->status.isActive = 1; /* 标记Shell为活动状态 */
            shell_handle_char(shell, ch);
            shell->status.isActive = 0; /* 重置活动状态 */
        }
    }
}

/**
 * @brief Shell字符处理函数 - 可在中断中安全调用
 * @param shell Shell实例指针
 * @param data 接收到的字符
 *
 * 说明：此函数设计为中断安全，可以直接在串口中断中调用
 * 用法示例：
 *   void USART1_IRQHandler(void)
 *   {
 *       if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
 *       {
 *           char ch = USART_ReceiveData(USART1);
 *           shellHandler(&g_shell, ch);
 *       }
 *   }
 */
void shellHandler(shell_t *shell, char data)
{
    if (shell == NULL)
    {
        return;
    }

    /* 直接调用字符处理函数 */
    shell->status.isActive = 1; /* 标记Shell为活动状态 */
    shell_handle_char(shell, data);
    shell->status.isActive = 0; /* 重置活动状态 */
}

/* ==================== 内置命令 ==================== */

#if SHELL_USING_AUTH
/**
 * @brief 显示所有用户
 */
int shell_cmd_show_users(int argc, char *argv[])
{

    shell_t *shell = shellGetCurrent();
    if (shell == NULL)
    {
        return -1;
    }
    shell_print(shell, "\r\nRegistered Users:\r\n");
    shell_print(shell, "----------------------------------------\r\n");
    for (uint16_t i = 0; i < g_user_count; i++)
    {
        shell_printf(shell, "  %-16s - %s\r\n",
                     g_users[i].username,
                     shell_get_auth_name(g_users[i].auth_level));
    }
    shell_print(shell, "----------------------------------------\r\n");
    shell_print(shell, "\r\n");
    return 0;
}
#endif

#if SHELL_USING_MULTI_INSTANCE
/**
 * @brief 显示所有shell实例
 */
int shell_cmd_show_shells(int argc, char *argv[])
{
    shell_t *shell = shellGetCurrent();
    if (shell == NULL)
    {
        return -1;
    }

    shell_print(shell, "\r\nRegistered Shell Instances:\r\n");
    shell_print(shell, "----------------------------------------\r\n");

    for (int i = 0; i < SHELL_MAX_INSTANCES; i++)
    {
        if (g_shell_instances[i] != NULL)
        {
            shell_printf(shell, "  %-16s - %s\r\n",
                         g_shell_instances[i]->name,
                         g_shell_instances[i]->status.isActive ? "Active" : "Inactive");
        }
    }

    shell_print(shell, "----------------------------------------\r\n");
    shell_print(shell, "\r\n");

    return 0;
}
#endif

/**
 * @brief 示例命令: version - 显示版本信息
 */
int shell_cmd_version(int argc, char *argv[])
{
    printf("\r\n");
    printf("Firmware Version: 1.0.0\r\n");
    printf("Build Date: %s %s\r\n", __DATE__, __TIME__);
    printf("MCU: STM32F103\r\n");
    printf("\r\n");
    return 0;
}

/**
 * @brief 示例命令: echo - 回显参数
 */
int shell_cmd_echo(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: echo <text>\r\n");
        return -1;
    }

    for (int i = 1; i < argc; i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\r\n");

    return 0;
}

/**
 * @brief help命令 - 显示所有可用命令
 */
int shell_cmd_help(int argc, char *argv[])
{
    shell_t *shell = shellGetCurrent();
    if (shell == NULL)
    {
        return -1;
    }

#if SHELL_USING_COMPLETION
    /* 复用命令列表显示函数 */
    shell_show_commands(shell, NULL, 1);
#else
    /* 未启用补全功能时的简化版本 */
    shell_print(shell, "\r\nAvailable commands:\r\n");
    shell_print(shell, "----------------------------------------\r\n");

    for (uint16_t i = 0; i < shell->cmd_count; i++)
    {
        shell_printf(shell, "  %-16s - %s\r\n",
                     shell->cmd_list[i].name,
                     shell->cmd_list[i].desc);
    }

    shell_print(shell, "----------------------------------------\r\n");
    shell_print(shell, "\r\n");
#endif

    return 0;
}

/**
 * @brief clear命令 - 清屏
 */
int shell_cmd_clear(int argc, char *argv[])
{
    shell_t *shell = shellGetCurrent();
    if (shell == NULL)
    {
        return -1;
    }

    shell_print(shell, ANSI_CLEAR_SCREEN);
    return 0;
}

#if SHELL_USING_HISTORY
/**
 * @brief history命令 - 显示历史记录
 */
int shell_cmd_history(int argc, char *argv[])
{
    shell_t *shell = shellGetCurrent();
    if (shell == NULL)
    {
        return -1;
    }

    shell_print(shell, "\r\nCommand history:\r\n");

    if (shell->history_count == 0)
    {
        shell_print(shell, "  (empty)\r\n");
    }
    else
    {
        uint8_t start = shell->history_index;
        for (uint8_t i = 0; i < shell->history_count; i++)
        {
            uint8_t idx = (start + SHELL_HISTORY_MAX - shell->history_count + i) % SHELL_HISTORY_MAX;
            shell_printf(shell, "  %2d: %s\r\n", i + 1, shell->history[idx]);
        }
    }

    shell_print(shell, "\r\n");
    return 0;
}
#endif

#if SHELL_USING_AUTH
/**
 * @brief logout命令 - 退出登录
 */
int shell_cmd_logout(int argc, char *argv[])
{
    shell_t *shell = shellGetCurrent();
    if (shell == NULL)
    {
        return -1;
    }

    if (shell->login_state == 3 && shell->current_user != NULL)
    {
        shell_printf(shell, "\r\nUser '%s' logged out.\r\n\r\n", shell->current_user->username);
        shell_logout(shell);
    }
    else
    {
        shell_print(shell, "\r\nNot logged in.\r\n\r\n");
    }

    return 0;
}
#endif

/* ====================================================================================
 *                              内置命令列表定义
 * ====================================================================================
 * 说明：
 * - 这些是Shell系统自带的基础命令
 * - 通过 shell_get_builtin_commands() 函数供外部使用
 * - 如需禁用某个内置命令，可以注释掉对应行
 * ==================================================================================== */

static const shell_cmd_t builtin_commands[] = {
    {"help", "Show all available commands", shell_cmd_help, SHELL_AUTH_GUEST},
    {"clear", "Clear screen", shell_cmd_clear, SHELL_AUTH_GUEST},
#if SHELL_USING_HISTORY
    {"history", "Show command history", shell_cmd_history, SHELL_AUTH_GUEST},
#endif
#if SHELL_USING_AUTH
    {"users", "Show registered users", shell_cmd_show_users, SHELL_AUTH_ADMIN},
    {"logout", "Logout current user", shell_cmd_logout, SHELL_AUTH_GUEST},
#endif
#if SHELL_USING_PASSTHROUGH
    {"enter_passthrough", "Enter passthrough mode", shell_cmd_enter_passthrough, SHELL_AUTH_USER},
    {"exit_passthrough", "Exit passthrough mode", shell_cmd_exit_passthrough, SHELL_AUTH_USER},
#endif
    {"version", "Show firmware version", shell_cmd_version, SHELL_AUTH_GUEST},
    {"echo", "Echo input text", shell_cmd_echo, SHELL_AUTH_GUEST},
#if SHELL_USING_MULTI_INSTANCE
    {"shells", "Show registered shell instances", shell_cmd_show_shells, SHELL_AUTH_ADMIN},
#endif
};

/**
 * @brief 获取内置命令列表
 * @param count 输出参数，返回命令数量
 * @return 内置命令列表指针
 */
const shell_cmd_t *shell_get_builtin_commands(uint16_t *count)
{
    if (count != NULL)
    {
        *count = sizeof(builtin_commands) / sizeof(shell_cmd_t);
    }
    return builtin_commands;
}

/**
 * @brief 日志输出 - 输出日志到当前Shell
 * @param buffer 日志内容
 * @param len 日志长度
 */
void shell_log(const char *buffer, int len)
{
    shell_log_to(shellGetCurrent(), buffer, len);
}

/**
 * @brief 日志输出 - 输出日志到指定Shell实例
 * @param shell 指定Shell实例
 * @param buffer 日志内容
 * @param len 日志长度
 */
void shell_log_to(shell_t *shell, const char *buffer, int len)
{
    if (shell == NULL || shell->write == NULL)
    {
        return;
    }

    /* 透传模式下不处理日志 */
    if (shell->status.passthrough == 1)
    {
        return;
    }

    if (shell->status.isActive == 1)
    {
        /* 如果Shell正在执行命令，直接输出日志 */
        shell->write(buffer, len);
        return;
    }
    /* 清除当前行 */
    shell_print(shell, ANSI_CLEAR_LINE);
    /* 输出日志内容 */
    shell->write(buffer, len);
    /* 重新显示提示符和当前命令 */
    shell_show_prompt(shell);
    shell->write(shell->cmd_buffer, shell->cmd_len);
    /* 移动光标到正确位置 */
    int move_count = shell->cmd_len - shell->cmd_pos;
    for (int i = 0; i < move_count; i++)
    {
        shell_print(shell, ANSI_MOVE_LEFT);
    }
    shell->status.isActive = 0; /* 重置活动状态 */
}
