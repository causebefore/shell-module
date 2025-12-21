/*
 * @Author: liu
 * @Date: 2025-12-09
 * @Description: Shell命令列表定义文件
 *
 * Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

#include "shell.h"
#include <stdio.h>
#include <string.h>

/* ====================================================================================
 *                              用户自定义命令区
 * ====================================================================================
 * 说明：
 * 1. 在此区域添加您自己的命令实现
 * 2. 命令函数签名: static int cmd_xxx(int argc, char *argv[])
 * 3. 返回值: 0=成功, -1=失败
 * 4. 添加完成后，在文件末尾的 user_commands[] 数组中注册命令
 * ==================================================================================== */

static int user_cmd_test_main(int argc, char *argv[])
{
    shell_t *shell = shellGetCurrent();
    shell_print(shell, "This is a test command in main form.\r\n");
    return 0;
}

/* ==================== 用户自定义命令列表 ==================== */
/**
 * 用户应用命令
 * - 在这里添加您的自定义命令
 * - 格式: {命令名, 描述, 函数指针, [权限], type, paramNum}
 *
 * type 类型：
 *   - SHELL_TYPE_CMD_MAIN: 标准main形式命令 int cmd(int argc, char *argv[])
 *   - SHELL_TYPE_CMD_FUNC: 参数适配模式 int cmd(param1, param2, ...)
 *
 * paramNum: 参数适配模式下的参数数量（SHELL_TYPE_CMD_FUNC时有效）
 *
 * 示例：
 *   - 标准命令: {"help", "Show help", cmd_help, SHELL_AUTH_GUEST, SHELL_TYPE_CMD_MAIN, 0}
 *   - 参数适配: {"led", "Control LED", cmd_led, SHELL_AUTH_USER, SHELL_TYPE_CMD_FUNC, 1}
 */
static const shell_cmd_t user_commands[] = {
/* ============ 示例命令 - 可根据需要保留或删除 ============ */
#if SHELL_USING_AUTH
    {"test", "test", user_cmd_test_main, SHELL_AUTH_GUEST, SHELL_TYPE_CMD_MAIN, 0},
#else
    {"passthrough", "Enter passthrough mode (Ctrl+] to exit)", user_cmd_test_main, SHELL_TYPE_CMD_MAIN, 0},
#endif

    /* ============ 在此添加您的命令 ============ */
    // 标准main形式命令示例:
    // {"gpio", "Read GPIO pin state", cmd_gpio_read, SHELL_AUTH_USER, SHELL_TYPE_CMD_MAIN, 0},

    // 参数适配模式命令示例 (函数签名: int cmd_led(int state)):
    // {"led", "Control LED", cmd_led, SHELL_AUTH_USER, SHELL_TYPE_CMD_FUNC, 1},
};

/* ==================== 最终命令列表（自动合并） ==================== */
/**
 * 完整的Shell命令列表
 * - 自动包含内置命令和用户命令
 * - 不要手动修改此数组，请在上面的 user_commands[] 中添加命令
 */
#define MAX_BUILTIN_COMMANDS 10 /* 内置命令最大数量（预留） */
static shell_cmd_t shell_commands_merged[MAX_BUILTIN_COMMANDS +
                                         sizeof(user_commands) / sizeof(shell_cmd_t)];

/* 导出的命令列表指针和数量 */
const shell_cmd_t *shell_commands = NULL;
uint16_t shell_cmd_count = 0;

/**
 * @brief 初始化命令列表（合并内置命令和用户命令）
 * @note 此函数由 shell_init() 自动调用，用户无需手动调用
 */
void shell_commands_init(void)
{
    uint16_t index = 0;
    uint16_t i;
    uint16_t builtin_count = 0;
    const shell_cmd_t *builtin_list;

    /* 获取内置命令列表 */
    builtin_list = shell_get_builtin_commands(&builtin_count);

    /* 复制内置命令 */
    for (i = 0; i < builtin_count; i++)
    {
        shell_commands_merged[index++] = builtin_list[i];
    }

    /* 复制用户命令 */
    for (i = 0; i < sizeof(user_commands) / sizeof(shell_cmd_t); i++)
    {
        shell_commands_merged[index++] = user_commands[i];
    }

    /* 设置导出变量 */
    shell_commands = shell_commands_merged;
    shell_cmd_count = index;
}