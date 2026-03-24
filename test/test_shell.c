/**
 * @file    test_shell.c
 * @brief   Shell模块 Unity 单元测试
 *
 * 覆盖范围:
 *   - shell_init          初始化
 *   - shell_print/printf  基础输出
 *   - shell_input         字符处理 (普通字符/退格/回车/Ctrl+C/ESC序列)
 *   - shell_rx_push/push_buf/rx_read  环形接收缓冲区
 *   - shell_task          主任务循环
 *   - shell_log           日志尾行输出
 *   - shell_set_passthrough/exit_passthrough  透传模式
 *   - shell_set_users/login/logout  用户认证
 *   - cmd_help/clear/history/var/vars/login/logout/whoami  内置命令
 *   - 边界条件 & 空指针防护
 *
 * 构建方式 (主机端GCC):
 *   cd test
 *   gcc -Wall -Wextra -I. -Iunity/src test_shell.c test_shell_mock.c unity/src/unity.c -o test_shell
 *   ./test_shell
 */

/*
 * ---- 宏覆盖策略 ----
 * 主机端测试无法使用链接器 section 收集变量/命令段，
 * 先包含 shell.h 获取类型定义，然后覆盖 SHELL_VAR_LIST / SHELL_VAR_COUNT 宏，
 * 再包含 shell.c (其中 #include "shell.h" 被 include guard 阻止)，
 * 使得 shell.c 中 find_var / complete_var / cmd_vars 使用测试数组。
 */
#include "../shell.h"

/* 前向声明测试变量表 */
extern const shell_var_t g_test_vars[];
extern uint16_t          g_test_var_count;

/* 覆盖变量段宏 */
#undef SHELL_VAR_LIST
#undef SHELL_VAR_COUNT
#define SHELL_VAR_LIST()  (g_test_vars)
#define SHELL_VAR_COUNT() (g_test_var_count)

/* 直接包含实现 (shell.c 顶部 #include "shell.h" 被 include guard 跳过) */
#include "test_shell_mock.h"
#include "unity.h"

#include <string.h>

#include "../shell.c"

/* ==================== 测试用变量 ==================== */

static int          s_test_int   = 42;
static unsigned int s_test_uint  = 100;
static float        s_test_float = 3.14f;
static uint8_t      s_test_bool  = 1;
static const char*  s_test_str   = "hello";

const shell_var_t g_test_vars[] = {
    {.name = "test_int",   .ptr = (void*) &s_test_int,   .type = SHELL_VAR_INT,    .readonly = 0},
    {.name = "test_uint",  .ptr = (void*) &s_test_uint,  .type = SHELL_VAR_UINT,   .readonly = 0},
    {.name = "test_float", .ptr = (void*) &s_test_float, .type = SHELL_VAR_FLOAT,  .readonly = 0},
    {.name = "test_bool",  .ptr = (void*) &s_test_bool,  .type = SHELL_VAR_BOOL,   .readonly = 0},
    {.name = "test_str",   .ptr = (void*) &s_test_str,   .type = SHELL_VAR_STRING, .readonly = 1},
};
uint16_t g_test_var_count = sizeof(g_test_vars) / sizeof(g_test_vars[0]);

/* ==================== 测试用命令表 ==================== */

static const char* s_mode_list[] = {"uart", "spi", "i2c", NULL};

static shell_cmd_t s_test_cmds[] = {
    {.name = "ok",      .desc = "always ok",      .func = test_cmd_ok,     .permission = 0,                .comp_list = NULL       },
    {.name = "fail",    .desc = "return error",   .func = test_cmd_fail,   .permission = 0,                .comp_list = NULL       },
    {.name = "record",  .desc = "record args",    .func = test_cmd_record, .permission = 0,                .comp_list = NULL       },
    {.name = "help",    .desc = "show help",      .func = cmd_help,        .permission = 0,                .comp_list = NULL       },
    {.name = "clear",   .desc = "clear screen",   .func = cmd_clear,       .permission = 0,                .comp_list = NULL       },
    {.name = "history", .desc = "show history",   .func = cmd_history,     .permission = 0,                .comp_list = NULL       },
    {.name = "var",     .desc = "read/write var", .func = cmd_var,         .permission = 0,                .comp_list = NULL       },
    {.name = "vars",    .desc = "list vars",      .func = cmd_vars,        .permission = 0,                .comp_list = NULL       },
    {.name = "login",   .desc = "login",          .func = cmd_login,       .permission = 0,                .comp_list = NULL       },
    {.name = "logout",  .desc = "logout",         .func = cmd_logout,      .permission = 0,                .comp_list = NULL       },
    {.name = "whoami",  .desc = "current user",   .func = cmd_whoami,      .permission = 0,                .comp_list = NULL       },
    {.name = "admin",   .desc = "admin only",     .func = test_cmd_ok,     .permission = SHELL_PERM_ADMIN, .comp_list = NULL       },
    {.name = "mode",    .desc = "set mode",       .func = test_cmd_ok,     .permission = 0,                .comp_list = s_mode_list},
};

#define TEST_CMD_COUNT (sizeof(s_test_cmds) / sizeof(s_test_cmds[0]))

/* ==================== 测试用用户表 ==================== */

static const shell_user_t s_test_users[] = {
    {.name = "admin", .password = (const char*) (uintptr_t) 0xB888BBCFU, .permission = SHELL_PERM_ADMIN},
    {.name = "user",  .password = (const char*) (uintptr_t) 0x0U,        .permission = SHELL_PERM_USER },
};

#define TEST_USER_COUNT (sizeof(s_test_users) / sizeof(s_test_users[0]))

/* ==================== 测试夹具 ==================== */

static shell_t s_sh;

static void setup_shell(void)
{
    mock_write_reset();
    shell_init(&s_sh, s_test_cmds, TEST_CMD_COUNT, mock_write, mock_read);
    shell_set_users(&s_sh, s_test_users, TEST_USER_COUNT);
    s_sh.is_inited = 1; /* 跳过 banner */
    s_test_int     = 42;
    s_test_uint    = 100;
    s_test_float   = 3.14f;
    s_test_bool    = 1;
    s_test_str     = "hello";
}

static void input_string(const char* str)
{
    while (*str)
    {
        shell_input(&s_sh, *str++);
    }
}

static void input_command(const char* cmd)
{
    input_string(cmd);
    shell_input(&s_sh, '\r');
}

void setUp(void)
{
    setup_shell();
}

void tearDown(void)
{
}

/* =================================================================
 * 测试组1: shell_init
 * ================================================================= */

void test_init_null_pointer(void)
{
    shell_init(NULL, s_test_cmds, TEST_CMD_COUNT, mock_write, mock_read);
}

void test_init_sets_callbacks(void)
{
    shell_t sh;
    shell_init(&sh, s_test_cmds, TEST_CMD_COUNT, mock_write, mock_read);
    TEST_ASSERT_EQUAL_PTR(mock_write, sh.write);
    TEST_ASSERT_EQUAL_PTR(mock_read, sh.read);
    TEST_ASSERT_EQUAL_UINT16(TEST_CMD_COUNT, sh.cmd_cnt);
    TEST_ASSERT_EQUAL_UINT8(0, sh.cmd_len);
    TEST_ASSERT_EQUAL_UINT8(0, sh.is_inited);
}

void test_init_clears_struct(void)
{
    shell_t sh;
    memset(&sh, 0xFF, sizeof(sh));
    shell_init(&sh, s_test_cmds, TEST_CMD_COUNT, mock_write, mock_read);
    TEST_ASSERT_EQUAL_UINT16(0, sh.cmd_len);
    TEST_ASSERT_EQUAL_UINT16(0, sh.cmd_pos);
    TEST_ASSERT_EQUAL_UINT8(0, sh.esc_state);
    TEST_ASSERT_EQUAL_UINT8(0, sh.is_active);
    TEST_ASSERT_EQUAL_UINT8(0, sh.is_inited);
}

/* =================================================================
 * 测试组2: shell_print / shell_printf
 * ================================================================= */

void test_print_normal(void)
{
    mock_write_reset();
    shell_print(&s_sh, "hello");
    TEST_ASSERT_EQUAL_STRING("hello", mock_write_get());
}

void test_print_null_shell(void)
{
    shell_print(NULL, "hello");
}

void test_print_null_str(void)
{
    shell_print(&s_sh, NULL);
}

void test_print_null_write(void)
{
    shell_t sh;
    shell_init(&sh, s_test_cmds, TEST_CMD_COUNT, NULL, NULL);
    shell_print(&sh, "hello");
}

void test_printf_normal(void)
{
    mock_write_reset();
    shell_printf(&s_sh, "val=%d", 123);
    TEST_ASSERT_TRUE(mock_write_contains("val=123"));
}

void test_printf_null_shell(void)
{
    shell_printf(NULL, "test");
}

/* =================================================================
 * 测试组3: 环形缓冲区
 * ================================================================= */

void test_rx_push_single(void)
{
    shell_rx_push(&s_sh, 'A');
    char buf[4];
    int  n = shell_rx_read(&s_sh, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_CHAR('A', buf[0]);
}

void test_rx_push_multiple(void)
{
    shell_rx_push(&s_sh, 'A');
    shell_rx_push(&s_sh, 'B');
    shell_rx_push(&s_sh, 'C');
    char buf[8];
    int  n = shell_rx_read(&s_sh, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_CHAR('A', buf[0]);
    TEST_ASSERT_EQUAL_CHAR('B', buf[1]);
    TEST_ASSERT_EQUAL_CHAR('C', buf[2]);
}

void test_rx_push_overflow(void)
{
    for (uint16_t i = 0; i < SHELL_RX_BUF_SIZE; i++)
    {
        shell_rx_push(&s_sh, (uint8_t) ('0' + (i % 10)));
    }
    char buf[SHELL_RX_BUF_SIZE];
    int  n = shell_rx_read(&s_sh, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(SHELL_RX_BUF_SIZE - 1, n);
}

void test_rx_push_buf_normal(void)
{
    const uint8_t data[] = {0x41, 0x42, 0x43, 0x44};
    shell_rx_push_buf(&s_sh, data, sizeof(data));
    char buf[8];
    int  n = shell_rx_read(&s_sh, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_MEMORY("ABCD", buf, 4);
}

void test_rx_push_buf_null(void)
{
    shell_rx_push_buf(&s_sh, NULL, 5);
    shell_rx_push_buf(NULL, (const uint8_t*) "AB", 2);
}

void test_rx_read_empty(void)
{
    char buf[8];
    int  n = shell_rx_read(&s_sh, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(0, n);
}

void test_rx_read_null_params(void)
{
    int n = shell_rx_read(NULL, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, n);
    char buf[8];
    n = shell_rx_read(&s_sh, buf, 0);
    TEST_ASSERT_EQUAL_INT(0, n);
}

void test_rx_push_null_shell(void)
{
    shell_rx_push(NULL, 'A');
}

/* =================================================================
 * 测试组4: shell_input — 字符处理
 * ================================================================= */

void test_input_null_shell(void)
{
    shell_input(NULL, 'A');
}

void test_input_normal_char(void)
{
    mock_write_reset();
    shell_input(&s_sh, 'A');
    TEST_ASSERT_EQUAL_UINT16(1, s_sh.cmd_len);
    TEST_ASSERT_EQUAL_CHAR('A', s_sh.cmd_buf[0]);
}

void test_input_printable_range(void)
{
    for (char c = 0x20; c < 0x7F; c++)
    {
        setup_shell();
        shell_input(&s_sh, c);
        TEST_ASSERT_EQUAL_UINT16(1, s_sh.cmd_len);
        TEST_ASSERT_EQUAL_CHAR(c, s_sh.cmd_buf[0]);
    }
}

void test_input_buffer_full(void)
{
    for (int i = 0; i < SHELL_CMD_SIZE - 1; i++)
    {
        shell_input(&s_sh, 'A');
    }
    TEST_ASSERT_EQUAL_UINT16(SHELL_CMD_SIZE - 1, s_sh.cmd_len);
    shell_input(&s_sh, 'B');
    TEST_ASSERT_EQUAL_UINT16(SHELL_CMD_SIZE - 1, s_sh.cmd_len);
}

void test_input_backspace_at_start(void)
{
    shell_input(&s_sh, KEY_BS);
    TEST_ASSERT_EQUAL_UINT16(0, s_sh.cmd_len);
}

void test_input_backspace_delete_char(void)
{
    input_string("AB");
    shell_input(&s_sh, KEY_BS);
    TEST_ASSERT_EQUAL_UINT16(1, s_sh.cmd_len);
    TEST_ASSERT_EQUAL_CHAR('A', s_sh.cmd_buf[0]);
}

void test_input_del_key(void)
{
    input_string("AB");
    shell_input(&s_sh, KEY_DEL);
    TEST_ASSERT_EQUAL_UINT16(1, s_sh.cmd_len);
}

void test_input_enter_executes(void)
{
    mock_write_reset();
    input_command("ok");
    TEST_ASSERT_FALSE(mock_write_contains("command not found"));
    TEST_ASSERT_EQUAL_UINT16(0, s_sh.cmd_len);
}

void test_input_enter_cmd_not_found(void)
{
    mock_write_reset();
    input_command("nonexistent");
    TEST_ASSERT_TRUE(mock_write_contains("command not found"));
}

void test_input_enter_empty_line(void)
{
    shell_input(&s_sh, '\r');
    TEST_ASSERT_EQUAL_UINT16(0, s_sh.cmd_len);
}

void test_input_ctrl_c(void)
{
    input_string("test");
    shell_input(&s_sh, KEY_CTRL_C);
    TEST_ASSERT_EQUAL_UINT16(0, s_sh.cmd_len);
}

void test_input_esc_cursor_right(void)
{
    input_string("ABC");
    /* 左移2次 */
    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'D');
    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'D');
    TEST_ASSERT_EQUAL_UINT16(1, s_sh.cmd_pos);
    /* 右移1次 */
    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'C');
    TEST_ASSERT_EQUAL_UINT16(2, s_sh.cmd_pos);
}

void test_input_esc_cursor_left_boundary(void)
{
    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'D');
    TEST_ASSERT_EQUAL_UINT16(0, s_sh.cmd_pos);
}

void test_input_esc_cursor_right_boundary(void)
{
    input_string("AB");
    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'C');
    TEST_ASSERT_EQUAL_UINT16(2, s_sh.cmd_pos);
}

void test_input_insert_at_middle(void)
{
    input_string("AC");
    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'D');
    shell_input(&s_sh, 'B');
    TEST_ASSERT_EQUAL_UINT16(3, s_sh.cmd_len);
    s_sh.cmd_buf[s_sh.cmd_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("ABC", s_sh.cmd_buf);
}

void test_input_backspace_at_middle(void)
{
    input_string("ABC");
    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'D');
    shell_input(&s_sh, KEY_BS);
    TEST_ASSERT_EQUAL_UINT16(2, s_sh.cmd_len);
    s_sh.cmd_buf[s_sh.cmd_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("AC", s_sh.cmd_buf);
}

/* =================================================================
 * 测试组5: 命令执行与参数解析
 * ================================================================= */

void test_exec_record_args(void)
{
    input_command("record arg1 arg2");
    TEST_ASSERT_EQUAL_INT(3, test_cmd_record_get_argc());
    TEST_ASSERT_EQUAL_STRING("record", test_cmd_record_get_argv(0));
    TEST_ASSERT_EQUAL_STRING("arg1", test_cmd_record_get_argv(1));
    TEST_ASSERT_EQUAL_STRING("arg2", test_cmd_record_get_argv(2));
}

void test_exec_error_code(void)
{
    mock_write_reset();
    input_command("fail 5");
    TEST_ASSERT_TRUE(mock_write_contains("error: 5"));
}

void test_exec_max_args(void)
{
    char cmd[SHELL_CMD_SIZE];
    int  pos = 0;
    /* "record" 作为第一个参数 */
    const char* prefix = "record";
    while (*prefix && pos < SHELL_CMD_SIZE - 3)
    {
        cmd[pos++] = *prefix++;
    }
    for (int i = 1; i < SHELL_ARG_MAX + 2 && pos < SHELL_CMD_SIZE - 3; i++)
    {
        cmd[pos++] = ' ';
        cmd[pos++] = (char) ('a' + (i % 26));
    }
    cmd[pos] = '\0';
    input_command(cmd);
    TEST_ASSERT_TRUE(test_cmd_record_get_argc() <= SHELL_ARG_MAX);
}

/* =================================================================
 * 测试组6: 历史记录
 * ================================================================= */

void test_history_add_and_navigate(void)
{
    input_command("ok");
    input_command("record");
    mock_write_reset();

    /* 上键: 最近的 "record" */
    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'A');
    s_sh.cmd_buf[s_sh.cmd_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("record", s_sh.cmd_buf);

    /* 再上键: "ok" */
    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'A');
    s_sh.cmd_buf[s_sh.cmd_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("ok", s_sh.cmd_buf);
}

void test_history_duplicate_reject(void)
{
    input_command("ok");
    input_command("ok"); /* 重复 */

    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'A');
    s_sh.cmd_buf[s_sh.cmd_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("ok", s_sh.cmd_buf);
}

void test_history_empty(void)
{
    /* 无历史时上键不崩溃 */
    shell_input(&s_sh, KEY_ESC);
    shell_input(&s_sh, '[');
    shell_input(&s_sh, 'A');
    TEST_ASSERT_EQUAL_UINT16(0, s_sh.cmd_len);
}

void test_cmd_history_output(void)
{
    input_command("ok");
    mock_write_reset();
    input_command("history");
    TEST_ASSERT_TRUE(mock_write_contains("History:"));
    TEST_ASSERT_TRUE(mock_write_contains("ok"));
}

/* =================================================================
 * 测试组7: Tab 补全
 * ================================================================= */

void test_completion_single_match(void)
{
    input_string("he");
    shell_input(&s_sh, KEY_TAB);
    s_sh.cmd_buf[s_sh.cmd_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("help", s_sh.cmd_buf);
}

void test_completion_no_match(void)
{
    input_string("xyz");
    shell_input(&s_sh, KEY_TAB);
    s_sh.cmd_buf[s_sh.cmd_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("xyz", s_sh.cmd_buf);
}

void test_completion_multiple_match(void)
{
    input_string("lo");
    mock_write_reset();
    shell_input(&s_sh, KEY_TAB);
    TEST_ASSERT_TRUE(mock_write_contains("login"));
    TEST_ASSERT_TRUE(mock_write_contains("logout"));
}

void test_completion_arg_list(void)
{
    input_string("mode u");
    shell_input(&s_sh, KEY_TAB);
    s_sh.cmd_buf[s_sh.cmd_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("mode uart", s_sh.cmd_buf);
}

void test_completion_var_name(void)
{
    input_string("var test_i");
    shell_input(&s_sh, KEY_TAB);
    s_sh.cmd_buf[s_sh.cmd_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("var test_int", s_sh.cmd_buf);
}

/* =================================================================
 * 测试组8: 变量读写
 * ================================================================= */

void test_var_read_int(void)
{
    mock_write_reset();
    input_command("var test_int");
    TEST_ASSERT_TRUE(mock_write_contains("test_int = 42"));
}

void test_var_write_int(void)
{
    input_command("var test_int 99");
    TEST_ASSERT_EQUAL_INT(99, s_test_int);
}

void test_var_write_hex(void)
{
    input_command("var test_uint 0xFF");
    TEST_ASSERT_EQUAL_UINT(255, s_test_uint);
}

void test_var_write_float(void)
{
    input_command("var test_float 1.5");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.5f, s_test_float);
}

void test_var_write_bool(void)
{
    input_command("var test_bool 0");
    TEST_ASSERT_EQUAL_UINT8(0, s_test_bool);
}

void test_var_readonly(void)
{
    mock_write_reset();
    input_command("var test_str new_val");
    TEST_ASSERT_TRUE(mock_write_contains("Cannot modify string variable") || mock_write_contains("readonly"));
}

void test_var_not_found(void)
{
    mock_write_reset();
    input_command("var nonexistent");
    TEST_ASSERT_TRUE(mock_write_contains("not found"));
}

void test_vars_list_all(void)
{
    mock_write_reset();
    input_command("vars");
    TEST_ASSERT_TRUE(mock_write_contains("test_int"));
    TEST_ASSERT_TRUE(mock_write_contains("test_float"));
    TEST_ASSERT_TRUE(mock_write_contains("test_str"));
}

void test_var_no_args(void)
{
    mock_write_reset();
    input_command("var");
    TEST_ASSERT_TRUE(mock_write_contains("Usage"));
}

/* =================================================================
 * 测试组9: 用户认证
 * ================================================================= */

void test_login_no_password_user(void)
{
    int ret = shell_login(&s_sh, "user", "");
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_NOT_NULL(s_sh.cur_user);
    TEST_ASSERT_EQUAL_STRING("user", s_sh.cur_user->name);
    TEST_ASSERT_EQUAL_UINT8(1, s_sh.is_checked);
}

void test_login_user_not_found(void)
{
    int ret = shell_login(&s_sh, "nobody", "pass");
    TEST_ASSERT_EQUAL_INT(-1, ret);
    TEST_ASSERT_NULL(s_sh.cur_user);
}

void test_login_null_params(void)
{
    int ret = shell_login(NULL, "admin", "pass");
    TEST_ASSERT_EQUAL_INT(-1, ret);
    ret = shell_login(&s_sh, NULL, "pass");
    TEST_ASSERT_EQUAL_INT(-1, ret);
}

void test_logout(void)
{
    shell_login(&s_sh, "user", "");
    shell_logout(&s_sh);
    TEST_ASSERT_NULL(s_sh.cur_user);
    TEST_ASSERT_EQUAL_UINT8(0, s_sh.is_checked);
}

void test_logout_null(void)
{
    shell_logout(NULL);
}

void test_permission_denied(void)
{
    s_sh.cur_user   = NULL;
    s_sh.is_checked = 0;
    mock_write_reset();
    input_command("admin");
    TEST_ASSERT_TRUE(mock_write_contains("Permission denied"));
}

void test_permission_insufficient(void)
{
    /* user 权限 SHELL_PERM_USER(0x01), admin命令要求 SHELL_PERM_ADMIN(0x02) */
    shell_login(&s_sh, "user", "");
    mock_write_reset();
    input_command("admin");
    TEST_ASSERT_TRUE(mock_write_contains("Permission denied"));
}

void test_cmd_whoami_logged_in(void)
{
    shell_login(&s_sh, "user", "");
    mock_write_reset();
    input_command("whoami");
    TEST_ASSERT_TRUE(mock_write_contains("user"));
}

void test_cmd_whoami_not_logged_in(void)
{
    mock_write_reset();
    input_command("whoami");
    TEST_ASSERT_TRUE(mock_write_contains("not logged in"));
}

void test_set_users_null(void)
{
    shell_set_users(NULL, s_test_users, 2);
}

/* =================================================================
 * 测试组10: 透传模式
 * ================================================================= */

static uint8_t s_pt_last_ch  = 0;
static int     s_pt_call_cnt = 0;

static void test_pt_handler(uint8_t ch)
{
    s_pt_last_ch = ch;
    s_pt_call_cnt++;
}

void test_passthrough_enter(void)
{
    s_pt_call_cnt = 0;
    shell_set_passthrough(&s_sh, test_pt_handler);
    TEST_ASSERT_EQUAL_UINT8(1, s_sh.passthrough);

    shell_input(&s_sh, 'X');
    TEST_ASSERT_EQUAL_UINT8('X', s_pt_last_ch);
    TEST_ASSERT_EQUAL_INT(1, s_pt_call_cnt);
}

void test_passthrough_exit(void)
{
    shell_set_passthrough(&s_sh, test_pt_handler);
    shell_input(&s_sh, KEY_CTRL_EXIT);
    TEST_ASSERT_EQUAL_UINT8(0, s_sh.passthrough);
    TEST_ASSERT_NULL(s_sh.pt_handler);
}

void test_passthrough_null_params(void)
{
    shell_set_passthrough(NULL, test_pt_handler);
    shell_set_passthrough(&s_sh, NULL);
}

void test_exit_passthrough_null(void)
{
    shell_exit_passthrough(NULL);
}

/* =================================================================
 * 测试组11: shell_task
 * ================================================================= */

void test_task_null_shell(void)
{
    shell_task(NULL);
}

void test_task_first_call_shows_banner(void)
{
    shell_t sh;
    shell_init(&sh, s_test_cmds, TEST_CMD_COUNT, mock_write, mock_read);
    mock_write_reset();
    mock_read_set("", 0);
    shell_task(&sh);
    TEST_ASSERT_TRUE(mock_write_contains("Shell"));
    TEST_ASSERT_EQUAL_UINT8(1, sh.is_inited);
}

void test_task_reads_and_processes(void)
{
    const char input[] = "ok\r";
    mock_read_set(input, sizeof(input) - 1);
    mock_write_reset();
    shell_task(&s_sh);
    TEST_ASSERT_FALSE(mock_write_contains("command not found"));
}

/* =================================================================
 * 测试组12: shell_log
 * ================================================================= */

void test_log_normal(void)
{
    mock_write_reset();
    shell_log("LOG!", 4);
    TEST_ASSERT_TRUE(mock_write_contains("LOG!"));
}

void test_log_null_params(void)
{
    shell_log(NULL, 5);
    shell_log("test", 0);
    shell_log("test", -1);
}

void test_log_no_global_shell(void)
{
    shell_t* saved = g_shell;
    g_shell        = NULL;
    shell_log("test", 4);
    g_shell = saved;
}

void test_log_during_command(void)
{
    s_sh.is_active = 1;
    mock_write_reset();
    shell_log("active-log", 10);
    TEST_ASSERT_TRUE(mock_write_contains("active-log"));
    s_sh.is_active = 0;
}

void test_log_max_length(void)
{
    char big[300];
    memset(big, 'X', sizeof(big));
    mock_write_reset();
    shell_log(big, 300);
    TEST_ASSERT_TRUE(mock_write_len() > 0);
}

/* =================================================================
 * 测试组13: 内置命令
 * ================================================================= */

void test_cmd_help(void)
{
    mock_write_reset();
    input_command("help");
    TEST_ASSERT_TRUE(mock_write_contains("Commands:"));
    TEST_ASSERT_TRUE(mock_write_contains("ok"));
    TEST_ASSERT_TRUE(mock_write_contains("help"));
}

void test_cmd_clear(void)
{
    mock_write_reset();
    input_command("clear");
    TEST_ASSERT_TRUE(mock_write_contains("\033[2J"));
}

void test_cmd_help_no_global(void)
{
    shell_t* saved = g_shell;
    g_shell        = NULL;
    char* argv[]   = {"help"};
    int   ret      = cmd_help(1, argv);
    TEST_ASSERT_EQUAL_INT(-1, ret);
    g_shell = saved;
}

void test_cmd_login_no_args(void)
{
    mock_write_reset();
    input_command("login");
    TEST_ASSERT_TRUE(mock_write_contains("Usage"));
}

void test_cmd_logout_via_command(void)
{
    shell_login(&s_sh, "user", "");
    mock_write_reset();
    input_command("logout");
    TEST_ASSERT_TRUE(mock_write_contains("Logged out"));
    TEST_ASSERT_NULL(s_sh.cur_user);
}

/* =================================================================
 * 主入口
 * ================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* 初始化 */
    RUN_TEST(test_init_null_pointer);
    RUN_TEST(test_init_sets_callbacks);
    RUN_TEST(test_init_clears_struct);

    /* 基础输出 */
    RUN_TEST(test_print_normal);
    RUN_TEST(test_print_null_shell);
    RUN_TEST(test_print_null_str);
    RUN_TEST(test_print_null_write);
    RUN_TEST(test_printf_normal);
    RUN_TEST(test_printf_null_shell);

    /* 环形缓冲区 */
    RUN_TEST(test_rx_push_single);
    RUN_TEST(test_rx_push_multiple);
    RUN_TEST(test_rx_push_overflow);
    RUN_TEST(test_rx_push_buf_normal);
    RUN_TEST(test_rx_push_buf_null);
    RUN_TEST(test_rx_read_empty);
    RUN_TEST(test_rx_read_null_params);
    RUN_TEST(test_rx_push_null_shell);

    /* 字符处理 */
    RUN_TEST(test_input_null_shell);
    RUN_TEST(test_input_normal_char);
    RUN_TEST(test_input_printable_range);
    RUN_TEST(test_input_buffer_full);
    RUN_TEST(test_input_backspace_at_start);
    RUN_TEST(test_input_backspace_delete_char);
    RUN_TEST(test_input_del_key);
    RUN_TEST(test_input_enter_executes);
    RUN_TEST(test_input_enter_cmd_not_found);
    RUN_TEST(test_input_enter_empty_line);
    RUN_TEST(test_input_ctrl_c);
    RUN_TEST(test_input_esc_cursor_right);
    RUN_TEST(test_input_esc_cursor_left_boundary);
    RUN_TEST(test_input_esc_cursor_right_boundary);
    RUN_TEST(test_input_insert_at_middle);
    RUN_TEST(test_input_backspace_at_middle);

    /* 命令执行 */
    RUN_TEST(test_exec_record_args);
    RUN_TEST(test_exec_error_code);
    RUN_TEST(test_exec_max_args);

    /* 历史记录 */
    RUN_TEST(test_history_add_and_navigate);
    RUN_TEST(test_history_duplicate_reject);
    RUN_TEST(test_history_empty);
    RUN_TEST(test_cmd_history_output);

    /* Tab补全 */
    RUN_TEST(test_completion_single_match);
    RUN_TEST(test_completion_no_match);
    RUN_TEST(test_completion_multiple_match);
    RUN_TEST(test_completion_arg_list);
    RUN_TEST(test_completion_var_name);

    /* 变量读写 */
    RUN_TEST(test_var_read_int);
    RUN_TEST(test_var_write_int);
    RUN_TEST(test_var_write_hex);
    RUN_TEST(test_var_write_float);
    RUN_TEST(test_var_write_bool);
    RUN_TEST(test_var_readonly);
    RUN_TEST(test_var_not_found);
    RUN_TEST(test_vars_list_all);
    RUN_TEST(test_var_no_args);

    /* 用户认证 */
    RUN_TEST(test_login_no_password_user);
    RUN_TEST(test_login_user_not_found);
    RUN_TEST(test_login_null_params);
    RUN_TEST(test_logout);
    RUN_TEST(test_logout_null);
    RUN_TEST(test_permission_denied);
    RUN_TEST(test_permission_insufficient);
    RUN_TEST(test_cmd_whoami_logged_in);
    RUN_TEST(test_cmd_whoami_not_logged_in);
    RUN_TEST(test_set_users_null);

    /* 透传模式 */
    RUN_TEST(test_passthrough_enter);
    RUN_TEST(test_passthrough_exit);
    RUN_TEST(test_passthrough_null_params);
    RUN_TEST(test_exit_passthrough_null);

    /* shell_task */
    RUN_TEST(test_task_null_shell);
    RUN_TEST(test_task_first_call_shows_banner);
    RUN_TEST(test_task_reads_and_processes);

    /* shell_log */
    RUN_TEST(test_log_normal);
    RUN_TEST(test_log_null_params);
    RUN_TEST(test_log_no_global_shell);
    RUN_TEST(test_log_during_command);
    RUN_TEST(test_log_max_length);

    /* 内置命令 */
    RUN_TEST(test_cmd_help);
    RUN_TEST(test_cmd_clear);
    RUN_TEST(test_cmd_help_no_global);
    RUN_TEST(test_cmd_login_no_args);
    RUN_TEST(test_cmd_logout_via_command);

    return UNITY_END();
}
