/**
 * @file    shell_user.c
 * @brief   用户命令、变量、用户表
 */

#include "shell.h"

#include <stddef.h>  /* NULL */
#include <string.h>

/* ==================== 用户命令示例 ==================== */

static int cmd_test(int argc, char* argv[])
{
    shell_print(g_shell, "Test OK\r\n");
    return 0;
}

static int cmd_reboot(int argc, char* argv[])
{
    shell_print(g_shell, "System rebooting...\r\n");
    /* HAL_NVIC_SystemReset(); */
    return 0;
}

static int cmd_mode(int argc, char* argv[])
{
    if (argc < 2)
    {
        shell_print(g_shell, "Usage: mode <speed|angle|torque>\r\n");
        return -1;
    }
    shell_printf(g_shell, "Mode set to: %s\r\n", argv[1]);
    return 0;
}

/* 补全列表 */
static const char* s_mode_opts[] = {"speed", "angle", "torque", NULL};

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

/* 用户命令 */
SHELL_EXPORT_CMD(test, "Test command", cmd_test, SHELL_PERM_USER);
SHELL_EXPORT_CMD(reboot, "System reboot", cmd_reboot, SHELL_PERM_ADMIN);
SHELL_EXPORT_CMD_LIST(mode, "Set FOC mode", cmd_mode, SHELL_PERM_NONE, s_mode_opts);

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

/* 设置用户表的便捷函数 */
void shell_user_init(shell_t* sh)
{
    shell_set_users(sh, s_shell_users, USER_COUNT);
    shell_set_password_verify(sh, shell_user_password_verify);
}

#endif /* SHELL_USING_AUTH */

/* ==================== 变量导出示例 ==================== */

#if SHELL_USING_VAR
static int         s_test_int   = 100;
static uint32_t    s_test_uint  = 0x12345678;
static float       s_test_float = 3.14f;
static uint8_t     s_test_bool  = 1;
static const char* s_version    = "1.0.0";

SHELL_EXPORT_VAR(test_int, &s_test_int, SHELL_VAR_INT);
SHELL_EXPORT_VAR(test_uint, &s_test_uint, SHELL_VAR_UINT);
SHELL_EXPORT_VAR(test_float, &s_test_float, SHELL_VAR_FLOAT);
SHELL_EXPORT_VAR(test_bool, &s_test_bool, SHELL_VAR_BOOL);
SHELL_EXPORT_VAR_RO(version, &s_version, SHELL_VAR_STRING);
#endif
