/**
 * @file    test_shell_mock.c
 * @brief   Shell模块测试用 mock/桩函数实现
 */
#include "test_shell_mock.h"

#include <stdlib.h>
#include <string.h>

/* ==================== 输出捕获 ==================== */

static char     s_tx_buf[MOCK_TX_BUF_SIZE];
static uint16_t s_tx_pos = 0;

void mock_write_reset(void)
{
    s_tx_pos    = 0;
    s_tx_buf[0] = '\0';
}

void mock_write(const char* data, uint16_t len)
{
    if (!data)
    {
        return;
    }
    for (uint16_t i = 0; i < len && s_tx_pos < MOCK_TX_BUF_SIZE - 1; i++)
    {
        s_tx_buf[s_tx_pos++] = data[i];
    }
    s_tx_buf[s_tx_pos] = '\0';
}

const char* mock_write_get(void)
{
    return s_tx_buf;
}

uint16_t mock_write_len(void)
{
    return s_tx_pos;
}

int mock_write_contains(const char* substr)
{
    if (!substr)
    {
        return 0;
    }
    return strstr(s_tx_buf, substr) != NULL;
}

/* ==================== 输入注入 ==================== */

static char     s_rx_buf[MOCK_RX_BUF_SIZE];
static uint16_t s_rx_len = 0;
static uint16_t s_rx_pos = 0;

void mock_read_set(const char* data, uint16_t len)
{
    if (!data || len == 0)
    {
        s_rx_len = 0;
        s_rx_pos = 0;
        return;
    }
    if (len > MOCK_RX_BUF_SIZE)
    {
        len = MOCK_RX_BUF_SIZE;
    }
    memcpy(s_rx_buf, data, len);
    s_rx_len = len;
    s_rx_pos = 0;
}

int mock_read(char* buf, uint16_t max_len)
{
    if (!buf || max_len == 0)
    {
        return 0;
    }
    uint16_t cnt = 0;
    while (s_rx_pos < s_rx_len && cnt < max_len)
    {
        buf[cnt++] = s_rx_buf[s_rx_pos++];
    }
    return (int) cnt;
}

/* ==================== 测试命令 ==================== */

int test_cmd_ok(int argc, char* argv[])
{
    (void) argc;
    (void) argv;
    return 0;
}

int test_cmd_fail(int argc, char* argv[])
{
    if (argc >= 2)
    {
        return atoi(argv[1]);
    }
    return -1;
}

#define RECORD_ARGV_MAX 8
#define RECORD_STR_SIZE 32

static int  s_record_argc = 0;
static char s_record_argv[RECORD_ARGV_MAX][RECORD_STR_SIZE];

int test_cmd_record(int argc, char* argv[])
{
    s_record_argc = argc;
    for (int i = 0; i < RECORD_ARGV_MAX; i++)
    {
        if (i < argc && argv[i])
        {
            strncpy(s_record_argv[i], argv[i], RECORD_STR_SIZE - 1);
            s_record_argv[i][RECORD_STR_SIZE - 1] = '\0';
        }
        else
        {
            s_record_argv[i][0] = '\0';
        }
    }
    return 0;
}

int test_cmd_record_get_argc(void)
{
    return s_record_argc;
}

const char* test_cmd_record_get_argv(int n)
{
    if (n < 0 || n >= RECORD_ARGV_MAX || s_record_argv[n][0] == '\0')
    {
        return NULL;
    }
    return s_record_argv[n];
}
