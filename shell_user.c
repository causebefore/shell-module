/**
 * @file    shell_user.c
 * @brief   项目级用户命令、变量和用户表 (STM32 FreeRTOS 适配)
 */

#include "shell.h"

#include <string.h>

/* ==================== 项目命令 ==================== */

static int cmd_reboot(int argc, char* argv[])
{
    (void) argc;
    (void) argv;
    shell_print(g_shell, "System rebooting...\r\n");
    /* HAL_NVIC_SystemReset(); */
    return 0;
}

static int cmd_test(int argc, char* argv[])
{
    (void) argc;
    (void) argv;
    shell_print(g_shell, "Test OK\r\n");
    return 0;
}

/* ==================== 宏注册命令 ==================== */

/* 内置命令 */
SHELL_EXPORT_CMD(help, "Show commands", cmd_help, SHELL_PERM_NONE);
SHELL_EXPORT_CMD(clear, "Clear screen", cmd_clear, SHELL_PERM_NONE);

#if SHELL_USING_HISTORY
SHELL_EXPORT_CMD(history, "Show history", cmd_history, SHELL_PERM_NONE);
#endif

#if SHELL_USING_VAR
SHELL_EXPORT_CMD(var, "Read/write variable", cmd_var, SHELL_PERM_NONE);
SHELL_EXPORT_CMD(vars, "List all variables", cmd_vars, SHELL_PERM_NONE);
#endif

#if SHELL_USING_AUTH
SHELL_EXPORT_CMD(login, "Login user", cmd_login, SHELL_PERM_NONE);
SHELL_EXPORT_CMD(logout, "Logout", cmd_logout, SHELL_PERM_NONE);
SHELL_EXPORT_CMD(whoami, "Current user", cmd_whoami, SHELL_PERM_NONE);
#endif

/* 项目命令 */
SHELL_EXPORT_CMD(reboot, "System reboot", cmd_reboot, SHELL_PERM_ADMIN);
SHELL_EXPORT_CMD(test, "Test command", cmd_test, SHELL_PERM_USER);

/* ==================== 用户表 ==================== */

#if SHELL_USING_AUTH

static const shell_user_t s_shell_users[] = {
    {"root",  SHELL_PERM_ROOT },
    {"admin", SHELL_PERM_ADMIN},
    {"guest", SHELL_PERM_USER },
};

#define USER_COUNT (sizeof(s_shell_users) / sizeof(s_shell_users[0]))

static int shell_user_password_verify(const shell_user_t* user, const char* input_password)
{
    if (!user || !input_password)
    {
        return -1;
    }

    if (strcmp(user->name, "root") == 0)
    {
        return (strcmp(input_password, "123456") == 0) ? 0 : -1;
    }
    if (strcmp(user->name, "admin") == 0)
    {
        return (strcmp(input_password, "admin") == 0) ? 0 : -1;
    }
    if (strcmp(user->name, "guest") == 0)
    {
        return (input_password[0] == '\0') ? 0 : -1;
    }

    return -1;
}

void shell_user_init(shell_t* sh)
{
    shell_set_users(sh, s_shell_users, USER_COUNT);
    shell_set_password_verify(sh, shell_user_password_verify);
}

#endif /* SHELL_USING_AUTH */
