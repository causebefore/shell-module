/**
 *
 * @file shell.c
 * @author liu (lbq08@foxmail.com)
 * @brief 精简CLI版本 (支持命令导出宏)
 *
 * @copyright Copyright (c) 2026 liu
 * For study and research only
 */
#include "shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHELL_NAME    "Shell"
#define SHELL_VERSION "1.0.0"
#define SHELL_AUTHOR  "Liu"
#define SHELL_YEAR    "2026"


/* ANSI控制码 */
#define ANSI_CLEAR   "\033[2J\033[H"
#define ANSI_CLEARLN "\033[2K\r"
#define ANSI_LEFT    "\033[1D"
#define ANSI_RIGHT   "\033[1C"


/* 输出字符串定义 (方便多语言/自定义) */
#define STR_CRLF            "\r\n"
#define STR_INDENT          "  "
#define STR_CTRL_C          "^C\r\n"
#define STR_SHELL_READY     "\r\nShell Ready.\r\n"
#define STR_PASSTHROUGH_ON  "\r\n[Passthrough ON, Ctrl+] to exit]\r\n"
#define STR_PASSTHROUGH_OFF "\r\n[Passthrough OFF]\r\n"
#define STR_COMMANDS        "\r\nCommands:\r\n"
#define STR_HISTORY         "\r\nHistory:\r\n"
#define STR_EMPTY           "  (empty)\r\n"
#define STR_READONLY        " (readonly)"
#define STR_NOT_LOGGED_IN   "(not logged in)\r\n"
#define STR_LOGGED_OUT      "Logged out\r\n"
#define STR_USER_NOT_FOUND  "User not found\r\n"
#define STR_PASSWORD_WRONG  "Password incorrect\r\n"
#define STR_PERM_DENIED     "Permission denied\r\n"
#define STR_VAR_READONLY    "Variable is readonly\r\n"
#define STR_VAR_CANT_MODIFY "Cannot modify string variable\r\n"
#define STR_USAGE_VAR       "Usage: var <name> [value]\r\n"
#define STR_USAGE_LOGIN     "Usage: login <user> [password]\r\n"
#define STR_VAR_NOT_FOUND   "Variable '%s' not found\r\n"
#define STR_CMD_NOT_FOUND   "%s: command not found\r\n"
#define STR_CMD_ERROR       "error: %d\r\n"
#define STR_WELCOME         "Welcome, %s!\r\n"
#define STR_BANNER                                                                                                     \
    "\r\n"                                                                                                             \
    "  " SHELL_NAME " v" SHELL_VERSION                                                                                 \
    "\r\n"                                                                                                             \
    "  Built: " __DATE__ " " __TIME__                                                                                  \
    "\r\n"                                                                                                             \
    "  Copyright (c) " SHELL_YEAR " " SHELL_AUTHOR                                                                     \
    "\r\n"                                                                                                             \
    "\r\n"
#define STR_WHOAMI    "%s (perm: 0x%02X)\r\n"
#define STR_VAR_COUNT "\r\nVariables (%d):\r\n"
#define STR_HIST_ITEM "  %2d: %s\r\n"
#define STR_HELP_ITEM "  %-12s %s\r\n"
#define STR_COMP_ITEM "  %s\r\n"

/* 全局shell */
shell_t* g_shell = NULL;

/* ==================== 基础输出 ==================== */

void shell_print(shell_t* sh, const char* str)
{
    if (sh && sh->write && str)
    {
        sh->write(str, strlen(str));
    }
}

void shell_printf(shell_t* sh, const char* fmt, ...)
{
    if (!sh || !sh->write || !fmt)
    {
        return;
    }
    char    buf[SHELL_PRINTF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int32_t len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (len > 0)
    {
        if (len > (int32_t) sizeof(buf) - 1)
        {
            len = sizeof(buf) - 1; /* 防止截断后len过大 */
        }
        sh->write(buf, len);
    }
}

/* ==================== 环形接收缓冲区 ==================== */
#if SHELL_RX_BUF_SIZE > 0

/**
 * @brief 向接收缓冲区写入单字节 (中断安全)
 * @note  在UART接收中断中调用
 */
void shell_rx_push(shell_t* sh, uint8_t ch)
{
    if (!sh)
    {
        return;
    }
    uint16_t next = (sh->rx_head + 1) & (SHELL_RX_BUF_SIZE - 1);
    if (next != sh->rx_tail) /* 缓冲区未满 */
    {
        sh->rx_buf[sh->rx_head] = ch;
        __asm volatile("" ::: "memory"); /* 确保数据写入先于 head 更新 */
        sh->rx_head = next;
    }
    /* 满则丢弃 */
}

/**
 * @brief 向接收缓冲区写入多字节 (中断安全)
 * @note  在DMA接收完成中断中调用
 */
void shell_rx_push_buf(shell_t* sh, const uint8_t* data, uint16_t len)
{
    if (!sh || !data)
    {
        return;
    }
    for (uint16_t i = 0; i < len; i++)
    {
        uint16_t next = (sh->rx_head + 1) & (SHELL_RX_BUF_SIZE - 1);
        if (next == sh->rx_tail)
        {
            break; /* 缓冲区满 */
        }
        sh->rx_buf[sh->rx_head] = data[i];
        __asm volatile("" ::: "memory"); /* 确保数据写入先于 head 更新 */
        sh->rx_head = next;
    }
}

/**
 * @brief 从接收缓冲区读取数据 (主循环调用)
 * @return 读取到的字节数
 */
int shell_rx_read(shell_t* sh, char* buf, uint16_t max_len)
{
    if (!sh || !buf || max_len == 0)
    {
        return 0;
    }

    uint16_t cnt  = 0;
    uint16_t tail = sh->rx_tail;
    uint16_t head = sh->rx_head;
    __asm volatile("" ::: "memory"); /* 确保先读取 head 再访问数据 */

    while (tail != head && cnt < max_len)
    {
        buf[cnt++] = sh->rx_buf[tail];
        tail       = (tail + 1) & (SHELL_RX_BUF_SIZE - 1);
    }
    __asm volatile("" ::: "memory"); /* 确保数据读取完成后再更新 tail */
    sh->rx_tail = tail;              /* 更新读位置 */
    return cnt;
}

#endif /* SHELL_RX_BUF_SIZE > 0 */

static void show_prompt(shell_t* sh)
{
#if SHELL_USING_AUTH
    if (sh->cur_user && sh->is_checked)
    {
        shell_print(sh, sh->cur_user->name);
        shell_print(sh, SHELL_PROMPT);
    }
    else
    {
        shell_print(sh, SHELL_PROMPT);
    }
#else
    shell_print(sh, SHELL_PROMPT);
#endif
}

static void refresh_line(shell_t* sh)
{
    shell_print(sh, ANSI_CLEARLN);
    show_prompt(sh);
    if (sh->cmd_len > 0 && sh->write)
    {
        sh->write(sh->cmd_buf, sh->cmd_len);
    }
    for (int16_t i = sh->cmd_len - sh->cmd_pos; i > 0; i--)
    {
        shell_print(sh, ANSI_LEFT);
    }
}

/* ==================== 历史记录 ==================== */

#if SHELL_USING_HISTORY

    #define STR_LOGIN_PREFIX "login "
    #define STR_LOGIN_MASKED "login ***"

static void hist_add(shell_t* sh, const char* cmd)
{
    if (!cmd || !cmd[0])
    {
        return;
    }
    if (sh->hist_cnt > 0)
    {
        uint8_t last = (sh->hist_idx == 0) ? SHELL_HISTORY_MAX - 1 : sh->hist_idx - 1;
        if (strcmp(sh->hist[last], cmd) == 0)
        {
            return;
        }
    }

    /* 过滤 login 命令中的密码，防止密码泄露到历史记录 */
    if (strncmp(cmd, STR_LOGIN_PREFIX, sizeof(STR_LOGIN_PREFIX) - 1) == 0)
    {
        strncpy(sh->hist[sh->hist_idx], STR_LOGIN_MASKED, SHELL_CMD_SIZE - 1);
    }
    else
    {
        strncpy(sh->hist[sh->hist_idx], cmd, SHELL_CMD_SIZE - 1);
    }
    sh->hist[sh->hist_idx][SHELL_CMD_SIZE - 1] = '\0';
    sh->hist_idx                               = (sh->hist_idx + 1) % SHELL_HISTORY_MAX;
    if (sh->hist_cnt < SHELL_HISTORY_MAX)
    {
        sh->hist_cnt++;
    }
}

static const char* hist_get(shell_t* sh, int8_t dir)
{
    if (!sh || sh->hist_cnt == 0)
    {
        return NULL;
    }

    /* dir > 0: 向上翻(更早的命令), dir < 0: 向下翻(更近的命令) */
    if (dir > 0)
    {
        /* 向上翻: 取更早的命令 */
        if (sh->hist_cur == 0)
        {
            sh->hist_cur = sh->hist_cnt; /* 回绕到最后 */
        }
        sh->hist_cur--;
    }
    else
    {
        /* 向下翻: 取更近的命令 */
        sh->hist_cur++;
        if (sh->hist_cur >= sh->hist_cnt)
        {
            sh->hist_cur = 0; /* 回绕到开头 */
        }
    }

    /* 计算实际索引: 从最新位置往回数 */
    uint8_t actual_idx = (sh->hist_idx + SHELL_HISTORY_MAX - sh->hist_cnt + sh->hist_cur) % SHELL_HISTORY_MAX;
    if (actual_idx >= SHELL_HISTORY_MAX)
    {
        return NULL; /* 防止越界 */
    }
    return sh->hist[actual_idx];
}
#endif

/* ==================== 命令补全 ==================== */

#if SHELL_USING_COMPLETION

/* 命令名补全 */
static void complete_cmd(shell_t* sh)
{
    const shell_cmd_t* match = NULL;
    uint16_t           cnt   = 0;

    for (uint16_t i = 0; i < sh->cmd_cnt; i++)
    {
        if (strncmp(sh->cmds[i].name, sh->cmd_buf, sh->cmd_len) == 0)
        {
            match = &sh->cmds[i];
            cnt++;
        }
    }

    if (cnt == 1)
    {
        strncpy(sh->cmd_buf, match->name, SHELL_CMD_SIZE - 1);
        sh->cmd_buf[SHELL_CMD_SIZE - 1] = '\0'; /* 确保null终止 */
        sh->cmd_len                     = strlen(sh->cmd_buf);
        sh->cmd_pos                     = sh->cmd_len;
        refresh_line(sh);
    }
    else if (cnt > 1)
    {
        shell_print(sh, STR_CRLF);
        for (uint16_t i = 0; i < sh->cmd_cnt; i++)
        {
            if (strncmp(sh->cmds[i].name, sh->cmd_buf, sh->cmd_len) == 0)
            {
                shell_printf(sh, STR_COMP_ITEM, sh->cmds[i].name);
            }
        }
        refresh_line(sh);
    }
}

/* 通用字符串列表补全 */
static void complete_list(shell_t* sh, const char** list, const char* partial, int16_t partial_len, int16_t arg_start)
{
    const char** match = NULL;
    uint16_t     mcnt  = 0;

    for (const char** item = list; *item; item++)
    {
        if (strncmp(*item, partial, partial_len) == 0)
        {
            match = item;
            mcnt++;
        }
    }

    if (mcnt == 1)
    {
        /* 唯一匹配: 替换参数部分 */
        if (arg_start >= 0 && arg_start < SHELL_CMD_SIZE - 1)
        {
            sh->cmd_buf[arg_start] = '\0';
            strncat(sh->cmd_buf, *match, SHELL_CMD_SIZE - arg_start - 1);
            sh->cmd_buf[SHELL_CMD_SIZE - 1] = '\0'; /* 确保null终止 */
            sh->cmd_len                     = strlen(sh->cmd_buf);
            sh->cmd_pos                     = sh->cmd_len;
            refresh_line(sh);
        }
    }
    else if (mcnt > 1)
    {
        /* 多个匹配: 列出候选 */
        shell_print(sh, STR_CRLF);
        for (const char** item = list; *item; item++)
        {
            if (strncmp(*item, partial, partial_len) == 0)
            {
                shell_printf(sh, STR_COMP_ITEM, *item);
            }
        }
        refresh_line(sh);
    }
}

    #if SHELL_USING_VAR
/* 变量名补全 (用于 var 命令) */
static void complete_var(shell_t* sh, const char* partial, int16_t partial_len, int16_t arg_start)
{
    const shell_var_t* vars  = SHELL_VAR_LIST();
    uint16_t           cnt   = SHELL_VAR_COUNT();
    const shell_var_t* match = NULL;
    uint16_t           mcnt  = 0;

    for (uint16_t i = 0; i < cnt; i++)
    {
        if (strncmp(vars[i].name, partial, partial_len) == 0)
        {
            match = &vars[i];
            mcnt++;
        }
    }

    if (mcnt == 1)
    {
        /* 唯一匹配: 替换参数部分 */
        if (arg_start >= 0 && arg_start < SHELL_CMD_SIZE - 1)
        {
            sh->cmd_buf[arg_start] = '\0';
            strncat(sh->cmd_buf, match->name, SHELL_CMD_SIZE - arg_start - 1);
            sh->cmd_buf[SHELL_CMD_SIZE - 1] = '\0'; /* 确保null终止 */
            sh->cmd_len                     = strlen(sh->cmd_buf);
            sh->cmd_pos                     = sh->cmd_len;
            refresh_line(sh);
        }
    }
    else if (mcnt > 1)
    {
        /* 多个匹配: 列出候选 */
        shell_print(sh, STR_CRLF);
        for (uint16_t i = 0; i < cnt; i++)
        {
            if (strncmp(vars[i].name, partial, partial_len) == 0)
            {
                shell_printf(sh, STR_COMP_ITEM, vars[i].name);
            }
        }
        refresh_line(sh);
    }
}
    #endif /* SHELL_USING_VAR */


static void do_completion(shell_t* sh)
{
    /* 查找第一个空格位置 */
    char* space = NULL;
    for (uint16_t i = 0; i < sh->cmd_len; i++)
    {
        if (sh->cmd_buf[i] == ' ')
        {
            space = &sh->cmd_buf[i];
            break;
        }
    }

    if (space == NULL)
    {
        /* 没有空格: 补全命令名 */
        complete_cmd(sh);
    }
    else
    {
        /* 有空格: 根据命令补全参数 */
        int16_t cmd_len = (int16_t) (space - sh->cmd_buf);

        /* 跳过空格找参数起始 */
        const char* arg = space + 1;
        while (*arg == ' ' && arg < sh->cmd_buf + sh->cmd_len)
        {
            arg++;
        }
        int16_t arg_start   = (int16_t) (arg - sh->cmd_buf);
        int16_t partial_len = (int16_t) (sh->cmd_len - arg_start);

    #if SHELL_USING_VAR
        /* var 命令: 补全变量名 */
        if (cmd_len == 3 && strncmp(sh->cmd_buf, "var", 3) == 0)
        {
            complete_var(sh, arg, partial_len, arg_start);
            return;
        }
    #endif
        /* 查找命令的补全列表 */
        for (uint16_t i = 0; i < sh->cmd_cnt; i++)
        {
            if ((int16_t) strlen(sh->cmds[i].name) == cmd_len && strncmp(sh->cmds[i].name, sh->cmd_buf, cmd_len) == 0)
            {
                if (sh->cmds[i].comp_list != NULL)
                {
                    complete_list(sh, sh->cmds[i].comp_list, arg, partial_len, arg_start);
                }
                return;
            }
        }
    }
}
#endif

/* ==================== 权限检查 ==================== */

#if SHELL_USING_AUTH
static int32_t check_permission(shell_t* sh, const shell_cmd_t* cmd)
{
    /* 权限为0表示无限制 */
    if (cmd->permission == 0)
    {
        return 1;
    }
    /* 未登录 */
    if (!sh->cur_user || !sh->is_checked)
    {
        return 0;
    }
    /* 检查权限掩码 */
    return (cmd->permission & sh->cur_user->permission) != 0;
}
#endif

/* ==================== 命令执行 ==================== */

static void exec_cmd(shell_t* sh)
{
    char*   argv[SHELL_ARG_MAX];
    int32_t argc = 0;
    char*   ptr  = sh->cmd_buf;

    while (*ptr && argc < SHELL_ARG_MAX)
    {
        while (*ptr == ' ' || *ptr == '\t')
        {
            ptr++;
        }
        if (!*ptr)
        {
            break;
        }
        argv[argc++] = ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\t')
        {
            ptr++;
        }
        if (*ptr)
        {
            *ptr++ = '\0';
        }
    }

    if (argc == 0)
    {
        return;
    }

    for (uint16_t i = 0; i < sh->cmd_cnt; i++)
    {
        if (strcmp(sh->cmds[i].name, argv[0]) == 0)
        {
#if SHELL_USING_AUTH
            /* 权限检查 */
            if (!check_permission(sh, &sh->cmds[i]))
            {
                shell_print(sh, STR_PERM_DENIED);
                return;
            }
#endif
            sh->is_active = 1; /* 命令执行中 */
            int32_t ret   = sh->cmds[i].func(argc, argv);
            sh->is_active = 0; /* 命令执行完毕 */
            if (ret != 0)
            {
                shell_printf(sh, STR_CMD_ERROR, ret);
            }
            return;
        }
    }
    shell_printf(sh, STR_CMD_NOT_FOUND, argv[0]);
}

/* ==================== ESC序列处理 ==================== */

static void handle_esc(shell_t* sh, char ch)
{
    sh->esc_buf[sh->esc_idx++] = ch;

    if (sh->esc_idx >= 2 && sh->esc_buf[0] == '[')
    {
        switch (sh->esc_buf[1])
        {
#if SHELL_USING_HISTORY
            case 'A':
                { /* 上 */
                    const char* entry = hist_get(sh, 1);
                    if (entry)
                    {
                        strncpy(sh->cmd_buf, entry, SHELL_CMD_SIZE - 1);
                        sh->cmd_len = strlen(sh->cmd_buf);
                        sh->cmd_pos = sh->cmd_len;
                        refresh_line(sh);
                    }
                }
                break;
            case 'B':
                { /* 下 */
                    const char* entry = hist_get(sh, -1);
                    if (entry)
                    {
                        strncpy(sh->cmd_buf, entry, SHELL_CMD_SIZE - 1);
                        sh->cmd_len = strlen(sh->cmd_buf);
                        sh->cmd_pos = sh->cmd_len;
                        refresh_line(sh);
                    }
                }
                break;
#endif
            case 'C': /* 右 */
                if (sh->cmd_pos < sh->cmd_len)
                {
                    sh->cmd_pos++;
                    shell_print(sh, ANSI_RIGHT);
                }
                break;
            case 'D': /* 左 */
                if (sh->cmd_pos > 0)
                {
                    sh->cmd_pos--;
                    shell_print(sh, ANSI_LEFT);
                }
                break;
            default:
                break;
        }
        sh->esc_state = 0;
        sh->esc_idx   = 0;
    }

    if (sh->esc_idx >= sizeof(sh->esc_buf))
    {
        sh->esc_state = 0;
        sh->esc_idx   = 0;
    }
}

/* ==================== 字符处理 ==================== */

void shell_input(shell_t* sh, char ch)
{
    if (!sh)
    {
        return;
    }

#if SHELL_USING_PASSTHROUGH
    if (sh->is_passthrough)
    {
        if (ch == KEY_CTRL_EXIT)
        {
            shell_exit_passthrough(sh);
            return;
        }
        if (sh->pt_handler)
        {
            sh->pt_handler((uint8_t) ch);
        }
        return;
    }
#endif

    if (sh->esc_state)
    {
        handle_esc(sh, ch);
        return;
    }

    switch (ch)
    {
        case KEY_ESC:
            sh->esc_state = 1;
            sh->esc_idx   = 0;
            break;

        case KEY_TAB:
#if SHELL_USING_COMPLETION
            do_completion(sh);
#endif
            break;

        case KEY_BS:
        case KEY_DEL:
            if (sh->cmd_pos > 0)
            {
                memmove(&sh->cmd_buf[sh->cmd_pos - 1], &sh->cmd_buf[sh->cmd_pos], sh->cmd_len - sh->cmd_pos);
                sh->cmd_pos--;
                sh->cmd_len--;
                sh->cmd_buf[sh->cmd_len] = '\0';
                refresh_line(sh);
            }
            break;

        case KEY_CR:
        case KEY_LF:
            shell_print(sh, STR_CRLF);
            sh->cmd_buf[sh->cmd_len] = '\0';
#if SHELL_USING_HISTORY
            hist_add(sh, sh->cmd_buf);
            sh->hist_cur = sh->hist_cnt; /* 重置到末尾，下次上键从最新开始 */
#endif
            exec_cmd(sh);
            sh->cmd_len    = 0;
            sh->cmd_pos    = 0;
            sh->cmd_buf[0] = '\0';
            show_prompt(sh);
            break;

        case KEY_CTRL_C:
            shell_print(sh, STR_CTRL_C);
            sh->cmd_len    = 0;
            sh->cmd_pos    = 0;
            sh->cmd_buf[0] = '\0';
            show_prompt(sh);
            break;

        default:
            if (ch >= 0x20 && ch < 0x7F && sh->cmd_len < SHELL_CMD_SIZE - 1)
            {
                if (sh->cmd_pos < sh->cmd_len)
                {
                    memmove(&sh->cmd_buf[sh->cmd_pos + 1], &sh->cmd_buf[sh->cmd_pos], sh->cmd_len - sh->cmd_pos);
                }
                sh->cmd_buf[sh->cmd_pos++] = ch;
                sh->cmd_len++;
                sh->cmd_buf[sh->cmd_len] = '\0';
                refresh_line(sh);
            }
            break;
    }
}

/* ==================== 初始化与任务 ==================== */

void shell_init(shell_t* sh, const shell_config_t* cfg)
{
    if (!sh || !cfg || !cfg->write)
    {
        return;
    }

    memset(sh, 0, sizeof(shell_t));
    sh->write = cfg->write;

#if SHELL_USING_CMD_EXPORT
    sh->cmds    = SHELL_CMD_LIST();
    sh->cmd_cnt = SHELL_CMD_COUNT();
#endif

#if SHELL_USING_AUTH
    sh->password_verify = cfg->password_verify;
#endif

    sh->is_inited = 1;
    g_shell       = sh;
}

void shell_task(shell_t* sh)
{
    if (!sh)
    {
        return;
    }

    if (!sh->is_inited)
    {
        sh->is_inited = 1;
        shell_print(sh, ANSI_CLEAR);
        shell_print(sh, STR_BANNER);
        show_prompt(sh);
    }

    char    buf[16];
    int32_t len = 0;

    /* 优先使用用户提供的read回调 */
    if (sh->read)
    {
        len = sh->read(buf, sizeof(buf));
        if (len < 0)
        {
            len = 0; /* 负值表示错误，忽略 */
        }
        else if (len > (int32_t) sizeof(buf))
        {
            len = sizeof(buf); /* 防止read返回异常值 */
        }
    }
#if SHELL_RX_BUF_SIZE > 0
    /* 无read回调时, 使用内置环形缓冲区 */
    else
    {
        len = shell_rx_read(sh, buf, sizeof(buf));
    }
#endif

    for (int32_t i = 0; i < len; i++)
    {
        shell_input(sh, buf[i]);
    }

#if SHELL_USING_LOG_QUEUE
    shell_log_drain(sh);
#endif
}

/* ==================== 日志输出(尾行模式) ==================== */

void shell_log(const char* buf, int len)
{
    shell_t* sh = g_shell;
    if (!sh || !sh->write || !buf || len <= 0)
    {
        return;
    }

    /* 限制最大长度 */
    if (len > 200)
    {
        len = 200;
    }

#if SHELL_USING_PASSTHROUGH
    if (sh->is_passthrough)
    {
        return;
    }
#endif

    /* 命令执行中: 直接输出日志，不清行不恢复 (避免闪烁) */
    if (sh->is_active)
    {
        sh->write(buf, len);
        return;
    }

    /* 快照共享状态，避免与 shell_input 并发访问不一致 */
    uint16_t snap_len = sh->cmd_len;
    uint16_t snap_pos = sh->cmd_pos;
    if (snap_len > SHELL_CMD_SIZE - 1)
    {
        snap_len = SHELL_CMD_SIZE - 1;
    }
    if (snap_pos > snap_len)
    {
        snap_pos = snap_len;
    }

    /*
     * 缓冲区大小: \r(1) + 日志(200) + 提示符(32) + 命令(SHELL_CMD_SIZE)
     *            + 清行ESC(3) + 光标恢复ESC(SHELL_CMD_SIZE*3)
     */
#define SHELL_LOG_BUF_SIZE (1 + 200 + 32 + SHELL_CMD_SIZE + 3 + SHELL_CMD_SIZE * 3)
    char    out_buf[SHELL_LOG_BUF_SIZE];
    int16_t pos = 0;

    /* 回到行首 */
    out_buf[pos++] = '\r';

    /* 日志内容 */
    for (int32_t i = 0; i < len && pos < (int16_t) sizeof(out_buf) - 1; i++)
    {
        out_buf[pos++] = buf[i];
    }

    /* 提示符 (包含用户名) */
#if SHELL_USING_AUTH
    if (sh->cur_user && sh->is_checked)
    {
        const char* name = sh->cur_user->name;
        while (*name && pos < (int16_t) sizeof(out_buf) - 1)
        {
            out_buf[pos++] = *name++;
        }
    }
#endif
    const char* prompt = SHELL_PROMPT;
    while (*prompt && pos < (int16_t) sizeof(out_buf) - 1)
    {
        out_buf[pos++] = *prompt++;
    }

    /* 当前命令 (使用快照长度) */
    for (uint16_t i = 0; i < snap_len && pos < (int16_t) sizeof(out_buf) - 1; i++)
    {
        out_buf[pos++] = sh->cmd_buf[i];
    }

    /* 清除光标到行尾的残留字符 */
    if (pos < (int16_t) sizeof(out_buf) - 4)
    {
        out_buf[pos++] = '\033';
        out_buf[pos++] = '[';
        out_buf[pos++] = 'K';
    }

    /* 光标定位 (移回当前位置, 使用快照) */
    for (uint16_t i = snap_len - snap_pos; i > 0 && pos < (int16_t) sizeof(out_buf) - 4; i--)
    {
        out_buf[pos++] = '\033';
        out_buf[pos++] = '[';
        out_buf[pos++] = 'D';
    }

    /* 一次性发送 */
    sh->write(out_buf, pos);
}

/* ==================== 透传模式 ==================== */

#if SHELL_USING_PASSTHROUGH
void shell_set_passthrough(shell_t* sh, void (*handler)(uint8_t))
{
    if (!sh || !handler)
    {
        return;
    }
    sh->pt_handler     = handler;
    sh->is_passthrough = 1;
    shell_print(sh, STR_PASSTHROUGH_ON);
}

void shell_exit_passthrough(shell_t* sh)
{
    if (!sh)
    {
        return;
    }
    sh->is_passthrough = 0;
    sh->pt_handler     = NULL;
    shell_print(sh, STR_PASSTHROUGH_OFF);
    show_prompt(sh);
}
#endif

/* ==================== 内置命令 ==================== */

int cmd_help(int argc, char* argv[])
{
    shell_t* sh = g_shell;
    if (!sh)
    {
        return -1;
    }

    shell_print(sh, STR_COMMANDS);
    for (uint16_t i = 0; i < sh->cmd_cnt; i++)
    {
        shell_printf(sh, STR_HELP_ITEM, sh->cmds[i].name, sh->cmds[i].desc);
    }
    shell_print(sh, STR_CRLF);
    return 0;
}

int cmd_clear(int argc, char* argv[])
{
    shell_t* sh = g_shell;
    if (!sh)
    {
        return -1;
    }
    shell_print(sh, ANSI_CLEAR);
    return 0;
}

#if SHELL_USING_HISTORY
int cmd_history(int argc, char* argv[])
{
    shell_t* sh = g_shell;
    if (!sh)
    {
        return -1;
    }

    shell_print(sh, STR_HISTORY);
    if (sh->hist_cnt == 0)
    {
        shell_print(sh, STR_EMPTY);
    }
    else
    {
        for (uint8_t i = 0; i < sh->hist_cnt; i++)
        {
            uint8_t idx = (sh->hist_idx + SHELL_HISTORY_MAX - sh->hist_cnt + i) % SHELL_HISTORY_MAX;
            shell_printf(sh, STR_HIST_ITEM, i + 1, sh->hist[idx]);
        }
    }
    shell_print(sh, STR_CRLF);
    return 0;
}
#endif

/* ==================== 变量读写 ==================== */

#if SHELL_USING_VAR

/* 解析字符串为数值 (支持十进制和十六进制) */
static int32_t parse_number(const char* str, uint8_t* is_float, float* fval)
{
    *is_float = 0;
    *fval     = 0.0f;

    if (!str || !str[0])
    {
        return 0;
    }

    /* 检查是否有小数点 */
    const char* scan = str;
    while (*scan)
    {
        if (*scan == '.')
        {
            *is_float = 1;
            *fval     = (float) atof(str);
            return 0;
        }
        scan++;
    }

    /* 整数: 支持 0x 前缀 */
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        return (int32_t) strtol(str, NULL, 16);
    }
    return (int32_t) atoi(str);
}

/* 查找变量 */
static const shell_var_t* find_var(const char* name)
{
    if (!name)
    {
        return NULL;
    }

    const shell_var_t* vars = SHELL_VAR_LIST();
    uint16_t           cnt  = SHELL_VAR_COUNT();

    for (uint16_t i = 0; i < cnt; i++)
    {
        if (vars[i].name && strcmp(vars[i].name, name) == 0)
        {
            return &vars[i];
        }
    }
    return NULL;
}

/* 打印变量值 */
static void print_var(shell_t* sh, const shell_var_t* var)
{
    switch (var->type)
    {
        case SHELL_VAR_INT:
            shell_printf(sh, "%s = %d", var->name, *(int*) var->ptr);
            break;
        case SHELL_VAR_UINT:
            shell_printf(sh, "%s = %u (0x%X)", var->name, *(unsigned int*) var->ptr, *(unsigned int*) var->ptr);
            break;
        case SHELL_VAR_FLOAT:
            shell_printf(sh, "%s = %.4f", var->name, *(float*) var->ptr);
            break;
        case SHELL_VAR_BOOL:
            shell_printf(sh, "%s = %s", var->name, (*(uint8_t*) var->ptr) ? "true" : "false");
            break;
        case SHELL_VAR_STRING:
            shell_printf(sh, "%s = \"%s\"", var->name, *(const char**) var->ptr);
            break;
        default:
            break;
    }
    if (var->is_readonly)
    {
        shell_print(sh, STR_READONLY);
    }
    shell_print(sh, STR_CRLF);
}

/* var 命令: 读写变量 */
int cmd_var(int argc, char* argv[])
{
    shell_t* sh = g_shell;
    if (!sh)
    {
        return -1;
    }

    if (argc < 2)
    {
        shell_print(sh, STR_USAGE_VAR);
        return -1;
    }

    const shell_var_t* var = find_var(argv[1]);
    if (!var)
    {
        shell_printf(sh, STR_VAR_NOT_FOUND, argv[1]);
        return -1;
    }

    /* 读取 */
    if (argc == 2)
    {
        print_var(sh, var);
        return 0;
    }

    /* 写入 */
    if (var->is_readonly)
    {
        shell_print(sh, STR_VAR_READONLY);
        return -1;
    }

    uint8_t is_float;
    float   fval;
    int32_t ival = parse_number(argv[2], &is_float, &fval);

    switch (var->type)
    {
        case SHELL_VAR_INT:
            *(int*) var->ptr = ival;
            break;
        case SHELL_VAR_UINT:
            *(unsigned int*) var->ptr = (unsigned int) ival;
            break;
        case SHELL_VAR_FLOAT:
            *(float*) var->ptr = is_float ? fval : (float) ival;
            break;
        case SHELL_VAR_BOOL:
            *(uint8_t*) var->ptr = (ival != 0) ? 1 : 0;
            break;
        case SHELL_VAR_STRING:
            shell_print(sh, STR_VAR_CANT_MODIFY);
            return -1;
        default:
            break;
    }

    print_var(sh, var);
    return 0;
}

/* vars 命令: 列出所有变量 */
int cmd_vars(int argc, char* argv[])
{
    shell_t* sh = g_shell;
    if (!sh)
    {
        return -1;
    }

    const shell_var_t* vars = SHELL_VAR_LIST();
    uint16_t           cnt  = SHELL_VAR_COUNT();

    shell_printf(sh, STR_VAR_COUNT, cnt);
    for (uint16_t i = 0; i < cnt; i++)
    {
        shell_print(sh, STR_INDENT);
        print_var(sh, &vars[i]);
    }
    shell_print(sh, STR_CRLF);
    return 0;
}

#endif /* SHELL_USING_VAR */

/* ==================== 用户认证 ==================== */

#if SHELL_USING_AUTH

    #if SHELL_USING_HASH_PWD
/*
 * 简单哈希函数 (DJB2)
 * 注意: DJB2 为非密码学哈希, 32位空间存在碰撞风险。
 * 此认证仅用于调试防误操作, 不适合安全关键场景。
 */
static uint32_t shell_hash(const char* str)
{
    uint32_t hash = 5381;
    int      ch;
    while ((ch = *str++))
    {
        hash = ((hash << 5) + hash) + ch; /* hash * 33 + ch */
    }
    return hash;
}
    #endif

void shell_set_users(shell_t* sh, const shell_user_t* users, uint8_t cnt)
{
    if (!sh)
    {
        return;
    }
    sh->users    = users;
    sh->user_cnt = cnt;
    /* 默认未登录状态 */
    sh->cur_user   = NULL;
    sh->is_checked = 0;
}

int shell_login(shell_t* sh, const char* name, const char* password)
{
    if (!sh || !sh->users || !name)
    {
        return -1;
    }

    for (uint8_t i = 0; i < sh->user_cnt; i++)
    {
        if (!sh->users[i].name)
        {
            continue;
        }
        if (strcmp(sh->users[i].name, name) == 0)
        {
    #if SHELL_USING_HASH_PWD
            /* 哈希密码验证 */
            uint32_t stored_hash = (uint32_t) (uintptr_t) sh->users[i].password;
            if (stored_hash == 0 || (password && shell_hash(password) == stored_hash))
    #else
            /* 明文密码验证 */
            if (sh->users[i].password[0] == '\0' || (password && strcmp(sh->users[i].password, password) == 0))
    #endif
            {
                sh->cur_user   = &sh->users[i];
                sh->is_checked = 1;
                return 0;
            }
            return -2; /* 密码错误 */
        }
    }
    return -1; /* 用户不存在 */
}

void shell_logout(shell_t* sh)
{
    if (!sh)
    {
        return;
    }
    sh->cur_user   = NULL;
    sh->is_checked = 0;
}

int cmd_login(int argc, char* argv[])
{
    shell_t* sh = g_shell;
    if (!sh)
    {
        return -1;
    }

    if (argc < 2)
    {
        shell_print(sh, STR_USAGE_LOGIN);
        return -1;
    }

    const char* password = (argc > 2) ? argv[2] : "";
    int32_t     ret      = shell_login(sh, argv[1], password);

    if (ret == 0)
    {
        shell_print(sh, STR_BANNER);
        shell_printf(sh, STR_WELCOME, sh->cur_user->name);
    }
    else if (ret == -2)
    {
        shell_print(sh, STR_PASSWORD_WRONG);
    }
    else
    {
        shell_print(sh, STR_USER_NOT_FOUND);
    }

    return ret;
}

int cmd_logout(int argc, char* argv[])
{
    shell_t* sh = g_shell;
    if (!sh)
    {
        return -1;
    }

    shell_logout(sh);
    shell_print(sh, STR_LOGGED_OUT);
    return 0;
}

int cmd_whoami(int argc, char* argv[])
{
    shell_t* sh = g_shell;
    if (!sh)
    {
        return -1;
    }

    if (sh->cur_user && sh->is_checked)
    {
        shell_printf(sh, STR_WHOAMI, sh->cur_user->name, sh->cur_user->permission);
    }
    else
    {
        shell_print(sh, STR_NOT_LOGGED_IN);
    }

    return 0;
}
#endif
