/**
 * @file    test_shell_ansi.c
 * @brief   ANSI颜色增强 Unity 单元测试
 *
 * 覆盖范围:
 *   - shell_print_color     基本功能 / 空颜色 / 空指针防护
 *   - shell_printf_color    基本功能 / 空颜色 / 空指针防护
 *
 * 构建方式 (主机端GCC):
 *   cd test
 *   gcc -Wall -Wextra -I. -Iunity/src -I.. test_shell_ansi.c test_shell_mock.c unity/src/unity.c -o test_shell_ansi
 *   ./test_shell_ansi
 */

#include "../shell.h"

/* 前向声明测试命令表 */
extern shell_cmd_t s_test_cmds[];
extern uint16_t    g_test_cmd_count;

#undef SHELL_CMD_LIST
#undef SHELL_CMD_COUNT
#define SHELL_CMD_LIST()  (s_test_cmds)
#define SHELL_CMD_COUNT() (g_test_cmd_count)

/* 前向声明测试变量表 */
extern const shell_var_t g_test_vars[];
extern uint16_t          g_test_var_count;

#undef SHELL_VAR_LIST
#undef SHELL_VAR_COUNT
#define SHELL_VAR_LIST()  (g_test_vars)
#define SHELL_VAR_COUNT() (g_test_var_count)

#include "test_shell_mock.h"
#include "unity.h"

#include <string.h>

#include "../shell.c"
#include "../shell_log.c"

/* ==================== 测试用命令表 ==================== */

shell_cmd_t s_test_cmds[] = {
    {.name = "ok", .desc = "always ok", .func = test_cmd_ok, .permission = 0, .comp_list = NULL},
};
uint16_t g_test_cmd_count = sizeof(s_test_cmds) / sizeof(s_test_cmds[0]);

/* ==================== 测试用变量表 ==================== */

const shell_var_t g_test_vars[] = {
    {.name = "dummy", .ptr = NULL, .type = SHELL_VAR_INT, .is_readonly = 0},
};
uint16_t g_test_var_count = 1;

/* ==================== 测试夹具 ==================== */

static shell_t s_sh;

static void setup_shell(void)
{
    mock_write_reset();
    shell_config_t cfg = {.write = mock_write};
    shell_init(&s_sh, &cfg);
    s_sh.is_inited = 1;
}

void setUp(void)
{
    setup_shell();
}

void tearDown(void)
{
}

/* =================================================================
 * 测试组1: shell_print_color 基本功能
 * ================================================================= */

void test_print_color_red(void)
{
    mock_write_reset();
    shell_print_color(&s_sh, ANSI_COLOR_RED, "ERR");
    /* 输出应包含颜色码、文本和重置码 */
    const char* out = mock_write_get();
    TEST_ASSERT_NOT_NULL(strstr(out, "ERR"));
    TEST_ASSERT_NOT_NULL(strstr(out, ANSI_COLOR_RESET));
}

void test_print_color_green(void)
{
    mock_write_reset();
    shell_print_color(&s_sh, ANSI_COLOR_GREEN, "OK");
    const char* out = mock_write_get();
    TEST_ASSERT_NOT_NULL(strstr(out, "OK"));
    TEST_ASSERT_NOT_NULL(strstr(out, ANSI_COLOR_RESET));
}

void test_print_color_contains_text(void)
{
    mock_write_reset();
    shell_print_color(&s_sh, ANSI_COLOR_BLUE, "HelloWorld");
    TEST_ASSERT_TRUE(mock_write_contains("HelloWorld"));
}

/* =================================================================
 * 测试组2: shell_print_color 空颜色
 * ================================================================= */

void test_print_color_null_color(void)
{
    mock_write_reset();
    shell_print_color(&s_sh, NULL, "NoColor");
    /* 应输出文本和重置码，但无颜色前缀 */
    const char* out = mock_write_get();
    TEST_ASSERT_NOT_NULL(strstr(out, "NoColor"));
    TEST_ASSERT_NOT_NULL(strstr(out, ANSI_COLOR_RESET));
}

void test_print_color_empty_string(void)
{
    mock_write_reset();
    shell_print_color(&s_sh, ANSI_COLOR_RED, "");
    /* 空字符串: 只输出颜色码和重置码 */
    const char* out = mock_write_get();
    TEST_ASSERT_NOT_NULL(strstr(out, ANSI_COLOR_RESET));
}

/* =================================================================
 * 测试组3: shell_print_color 空指针防护
 * ================================================================= */

void test_print_color_null_shell(void)
{
    shell_print_color(NULL, ANSI_COLOR_RED, "test");
}

void test_print_color_null_str(void)
{
    shell_print_color(&s_sh, ANSI_COLOR_RED, NULL);
}

void test_print_color_null_write(void)
{
    shell_t sh;
    memset(&sh, 0, sizeof(sh));
    shell_print_color(&sh, ANSI_COLOR_RED, "test");
}

/* =================================================================
 * 测试组4: shell_printf_color 基本功能
 * ================================================================= */

void test_printf_color_basic(void)
{
    mock_write_reset();
    shell_printf_color(&s_sh, ANSI_COLOR_RED, "val=%d", 42);
    const char* out = mock_write_get();
    TEST_ASSERT_NOT_NULL(strstr(out, "val=42"));
    TEST_ASSERT_NOT_NULL(strstr(out, ANSI_COLOR_RESET));
}

void test_printf_color_string_format(void)
{
    mock_write_reset();
    shell_printf_color(&s_sh, ANSI_COLOR_GREEN, "name=%s", "test");
    TEST_ASSERT_TRUE(mock_write_contains("name=test"));
}

void test_printf_color_yellow_warning(void)
{
    mock_write_reset();
    shell_printf_color(&s_sh, ANSI_COLOR_YELLOW, "warn: %d%%", 99);
    TEST_ASSERT_TRUE(mock_write_contains("warn: 99%"));
}

/* =================================================================
 * 测试组5: shell_printf_color 空参数
 * ================================================================= */

void test_printf_color_null_color(void)
{
    mock_write_reset();
    shell_printf_color(&s_sh, NULL, "msg=%d", 7);
    const char* out = mock_write_get();
    TEST_ASSERT_NOT_NULL(strstr(out, "msg=7"));
    TEST_ASSERT_NOT_NULL(strstr(out, ANSI_COLOR_RESET));
}

void test_printf_color_null_shell(void)
{
    shell_printf_color(NULL, ANSI_COLOR_RED, "test");
}

void test_printf_color_null_fmt(void)
{
    shell_printf_color(&s_sh, ANSI_COLOR_RED, NULL);
}

void test_printf_color_null_write(void)
{
    shell_t sh;
    memset(&sh, 0, sizeof(sh));
    shell_printf_color(&sh, ANSI_COLOR_RED, "test");
}

/* =================================================================
 * 主入口
 * ================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* shell_print_color 基本功能 */
    RUN_TEST(test_print_color_red);
    RUN_TEST(test_print_color_green);
    RUN_TEST(test_print_color_contains_text);

    /* shell_print_color 空颜色 */
    RUN_TEST(test_print_color_null_color);
    RUN_TEST(test_print_color_empty_string);

    /* shell_print_color 空指针防护 */
    RUN_TEST(test_print_color_null_shell);
    RUN_TEST(test_print_color_null_str);
    RUN_TEST(test_print_color_null_write);

    /* shell_printf_color 基本功能 */
    RUN_TEST(test_printf_color_basic);
    RUN_TEST(test_printf_color_string_format);
    RUN_TEST(test_printf_color_yellow_warning);

    /* shell_printf_color 空参数 */
    RUN_TEST(test_printf_color_null_color);
    RUN_TEST(test_printf_color_null_shell);
    RUN_TEST(test_printf_color_null_fmt);
    RUN_TEST(test_printf_color_null_write);

    return UNITY_END();
}
