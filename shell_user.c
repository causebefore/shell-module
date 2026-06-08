/**
 * @file    shell_user.c
 * @brief   内置命令注册
 *
 * 仅注册 shell 核心内置命令。项目特定的用户表、密码验证、
 * 自定义命令和变量导出应在适配层（port）中实现。
 */

#include "shell.h"

/* ==================== 内置命令注册 ==================== */

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
