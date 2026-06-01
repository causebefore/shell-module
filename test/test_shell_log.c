/**
 * @file    test_shell_log.c
 * @brief   ISR日志队列 Unity 单元测试
 *
 * 覆盖范围:
 *   - shell_log_isr          基本功能 / 空指针防护 / 缓冲区满丢弃
 *   - shell_log_text_isr     基本功能 / 空指针防护
 *   - shell_log_drain        基本功能 / 空队列 / 丢弃报告
 *
 * 构建方式 (主机端GCC):
 *   cd test
 *   gcc -Wall -Wextra -I. -Iunity/src -I.. test_shell_log.c test_shell_mock.c unity/src/unity.c -o test_shell_log
 *   ./test_shell_log
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
static uint16_t s_lock_count;
static uint16_t s_unlock_count;
static uint32_t s_last_unlock_state;

static uint32_t test_critical_enter(void)
{
    s_lock_count++;
    return 0x55AAu;
}

static void test_critical_exit(uint32_t state)
{
    s_unlock_count++;
    s_last_unlock_state = state;
}

static void setup_shell(void)
{
    mock_write_reset();
    shell_config_t cfg = {.write = mock_write};
    shell_init(&s_sh, &cfg);
    s_sh.read      = mock_read;
    s_sh.is_inited = 1;
    s_lock_count   = 0;
    s_unlock_count = 0;
    s_last_unlock_state = 0;
}

void setUp(void)
{
    setup_shell();
}

void tearDown(void)
{
}

/* =================================================================
 * 测试组1: shell_log_isr 基本功能
 * ================================================================= */

void test_log_isr_write_basic(void)
{
    int written = shell_log_isr(&s_sh, (const uint8_t*) "ABC", 3);
    TEST_ASSERT_EQUAL_INT(3, written);
    /* 数据在队列中，尚未输出 */
    TEST_ASSERT_EQUAL_UINT16(0, mock_write_len());
}

void test_log_isr_write_and_drain(void)
{
    mock_write_reset();
    shell_log_isr(&s_sh, (const uint8_t*) "ISR!", 4);
    shell_log_drain(&s_sh);
    TEST_ASSERT_TRUE(mock_write_contains("ISR!"));
}

void test_log_isr_multiple_writes(void)
{
    mock_write_reset();
    shell_log_isr(&s_sh, (const uint8_t*) "AB", 2);
    shell_log_isr(&s_sh, (const uint8_t*) "CD", 2);
    shell_log_drain(&s_sh);
    TEST_ASSERT_TRUE(mock_write_contains("ABCD"));
}

void test_log_isr_uses_configured_critical_section(void)
{
    shell_config_t cfg = {
        .write = mock_write,
        .critical_enter = test_critical_enter,
        .critical_exit = test_critical_exit,
    };
    shell_init(&s_sh, &cfg);

    int written = shell_log_isr(&s_sh, (const uint8_t*) "AB", 2);

    TEST_ASSERT_EQUAL_INT(2, written);
    TEST_ASSERT_EQUAL_UINT16(1, s_lock_count);
    TEST_ASSERT_EQUAL_UINT16(1, s_unlock_count);
    TEST_ASSERT_EQUAL_UINT32(0x55AAu, s_last_unlock_state);
}

/* =================================================================
 * 测试组2: shell_log_isr 空指针防护
 * ================================================================= */

void test_log_isr_null_shell(void)
{
    int r = shell_log_isr(NULL, (const uint8_t*) "x", 1);
    TEST_ASSERT_EQUAL_INT(0, r);
}

void test_log_isr_null_data(void)
{
    int r = shell_log_isr(&s_sh, NULL, 1);
    TEST_ASSERT_EQUAL_INT(0, r);
}

void test_log_isr_zero_len(void)
{
    int r = shell_log_isr(&s_sh, (const uint8_t*) "x", 0);
    TEST_ASSERT_EQUAL_INT(0, r);
}

/* =================================================================
 * 测试组3: shell_log_isr 缓冲区满时的丢弃行为
 * ================================================================= */

void test_log_isr_overflow_discard(void)
{
    /* 填满队列 */
    for (uint16_t i = 0; i < SHELL_LOG_QUEUE_SIZE + 10; i++)
    {
        shell_log_isr(&s_sh, (const uint8_t*) "A", 1);
    }
    /* 应有丢弃 */
    TEST_ASSERT_TRUE(s_sh.log_dropped_total > 0);
}

void test_log_isr_overflow_partial_write(void)
{
    /* 大块数据写入，队列容量有限 */
    uint8_t big[SHELL_LOG_QUEUE_SIZE + 64];
    memset(big, 'X', sizeof(big));
    int written = shell_log_isr(&s_sh, big, sizeof(big));
    /* 写入字节数应小于请求的 */
    TEST_ASSERT_TRUE(written < (int) sizeof(big));
    /* 应记录丢弃 */
    TEST_ASSERT_TRUE(s_sh.log_dropped_total > 0);
}

void test_log_isr_drop_counter_does_not_wrap_at_uint16(void)
{
    for (uint32_t i = 0; i < 70000u; i++)
    {
        shell_log_isr(&s_sh, (const uint8_t*) "A", 1);
    }

    TEST_ASSERT_TRUE(s_sh.log_dropped_total > 65535u);
}

void test_log_isr_overflow_drain_reports_drop(void)
{
    /* 填满队列并触发丢弃 */
    for (uint16_t i = 0; i < SHELL_LOG_QUEUE_SIZE + 10; i++)
    {
        shell_log_isr(&s_sh, (const uint8_t*) "A", 1);
    }
    TEST_ASSERT_TRUE(s_sh.log_dropped_total > 0);

    /* drain 应输出丢弃报告 */
    mock_write_reset();
    shell_log_drain(&s_sh);
    TEST_ASSERT_TRUE(mock_write_contains("dropped"));
}

/* =================================================================
 * 测试组4: shell_log_text_isr 基本功能
 * ================================================================= */

void test_log_text_isr_basic(void)
{
    mock_write_reset();
    int written = shell_log_text_isr(&s_sh, "HelloISR");
    TEST_ASSERT_EQUAL_INT(8, written);
    shell_log_drain(&s_sh);
    TEST_ASSERT_TRUE(mock_write_contains("HelloISR"));
}

void test_log_text_isr_empty_string(void)
{
    int written = shell_log_text_isr(&s_sh, "");
    /* 空字符串: len=0, 传给 shell_log_isr 返回0 */
    TEST_ASSERT_EQUAL_INT(0, written);
}

/* =================================================================
 * 测试组5: shell_log_text_isr 空指针防护
 * ================================================================= */

void test_log_text_isr_null_shell(void)
{
    int r = shell_log_text_isr(NULL, "test");
    TEST_ASSERT_EQUAL_INT(0, r);
}

void test_log_text_isr_null_string(void)
{
    int r = shell_log_text_isr(&s_sh, NULL);
    TEST_ASSERT_EQUAL_INT(0, r);
}

/* =================================================================
 * 测试组6: shell_log_drain 基本功能
 * ================================================================= */

void test_log_drain_empty_queue(void)
{
    mock_write_reset();
    shell_log_drain(&s_sh);
    TEST_ASSERT_EQUAL_UINT16(0, mock_write_len());
}

void test_log_drain_null_shell(void)
{
    shell_log_drain(NULL);
}

void test_log_drain_no_write_callback(void)
{
    shell_t sh;
    memset(&sh, 0, sizeof(sh));
    shell_log_drain(&sh);
}

/* =================================================================
 * 测试组7: shell_log_drain 丢弃报告
 * ================================================================= */

void test_log_drain_drop_report_once(void)
{
    /* 触发丢弃 */
    for (uint16_t i = 0; i < SHELL_LOG_QUEUE_SIZE + 10; i++)
    {
        shell_log_isr(&s_sh, (const uint8_t*) "A", 1);
    }
    /* 第一次 drain: 应输出丢弃报告 */
    mock_write_reset();
    shell_log_drain(&s_sh);
    TEST_ASSERT_TRUE(mock_write_contains("dropped"));

    /* 第二次 drain: 不应重复输出丢弃报告 (队列为空) */
    mock_write_reset();
    shell_log_drain(&s_sh);
    TEST_ASSERT_FALSE(mock_write_contains("dropped"));
}

void test_log_drain_wrap_around(void)
{
    /* 写入部分数据并 drain，制造环绕条件 */
    mock_write_reset();
    shell_log_isr(&s_sh, (const uint8_t*) "ABC", 3);
    shell_log_drain(&s_sh);
    TEST_ASSERT_TRUE(mock_write_contains("ABC"));

    /* 再写入数据，tail在中间，head可能环绕 */
    mock_write_reset();
    shell_log_text_isr(&s_sh, "XYZ");
    shell_log_drain(&s_sh);
    TEST_ASSERT_TRUE(mock_write_contains("XYZ"));
}

/* =================================================================
 * 主入口
 * ================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* shell_log_isr 基本功能 */
    RUN_TEST(test_log_isr_write_basic);
    RUN_TEST(test_log_isr_write_and_drain);
    RUN_TEST(test_log_isr_multiple_writes);
    RUN_TEST(test_log_isr_uses_configured_critical_section);

    /* shell_log_isr 空指针防护 */
    RUN_TEST(test_log_isr_null_shell);
    RUN_TEST(test_log_isr_null_data);
    RUN_TEST(test_log_isr_zero_len);

    /* shell_log_isr 缓冲区满丢弃 */
    RUN_TEST(test_log_isr_overflow_discard);
    RUN_TEST(test_log_isr_overflow_partial_write);
    RUN_TEST(test_log_isr_drop_counter_does_not_wrap_at_uint16);
    RUN_TEST(test_log_isr_overflow_drain_reports_drop);

    /* shell_log_text_isr 基本功能 */
    RUN_TEST(test_log_text_isr_basic);
    RUN_TEST(test_log_text_isr_empty_string);

    /* shell_log_text_isr 空指针防护 */
    RUN_TEST(test_log_text_isr_null_shell);
    RUN_TEST(test_log_text_isr_null_string);

    /* shell_log_drain 基本功能 */
    RUN_TEST(test_log_drain_empty_queue);
    RUN_TEST(test_log_drain_null_shell);
    RUN_TEST(test_log_drain_no_write_callback);

    /* shell_log_drain 丢弃报告 */
    RUN_TEST(test_log_drain_drop_report_once);
    RUN_TEST(test_log_drain_wrap_around);

    return UNITY_END();
}
