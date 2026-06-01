/**
 * @file    test_shell_port_adapter.c
 * @brief   Shell port adapter Unity tests
 */

#include "../shell.h"

extern shell_cmd_t s_test_cmds[];
extern uint16_t    g_test_cmd_count;

#undef SHELL_CMD_LIST
#undef SHELL_CMD_COUNT
#define SHELL_CMD_LIST()  (s_test_cmds)
#define SHELL_CMD_COUNT() (g_test_cmd_count)

extern const shell_var_t g_test_vars[];
extern uint16_t          g_test_var_count;

#undef SHELL_VAR_LIST
#undef SHELL_VAR_COUNT
#define SHELL_VAR_LIST()  (g_test_vars)
#define SHELL_VAR_COUNT() (g_test_var_count)

#include "../shell_port.h"
#include "test_shell_mock.h"
#include "unity.h"

#include <string.h>

#include "../shell.c"
#include "../shell_log.c"
#include "../shell_port.c"

shell_cmd_t s_test_cmds[] = {
    {.name = "ok", .desc = "always ok", .func = test_cmd_ok, .permission = 0, .comp_list = NULL},
};
uint16_t g_test_cmd_count = sizeof(s_test_cmds) / sizeof(s_test_cmds[0]);

const shell_var_t g_test_vars[] = {
    {.name = "dummy", .ptr = NULL, .type = SHELL_VAR_INT, .is_readonly = 0},
};
uint16_t g_test_var_count = 1;

typedef struct
{
    char     tx[128];
    uint16_t tx_len;
    uint16_t start_count;
    uint16_t enter_count;
    uint16_t exit_count;
    uint32_t last_exit_state;
    int      start_result;
} fake_backend_t;

static fake_backend_t s_fake;

static void fake_write(void* ctx, const char* data, uint16_t len)
{
    fake_backend_t* fake = (fake_backend_t*) ctx;
    if (len > sizeof(fake->tx) - fake->tx_len - 1u)
    {
        len = (uint16_t) (sizeof(fake->tx) - fake->tx_len - 1u);
    }
    memcpy(&fake->tx[fake->tx_len], data, len);
    fake->tx_len += len;
    fake->tx[fake->tx_len] = '\0';
}

static uint32_t fake_critical_enter(void* ctx)
{
    fake_backend_t* fake = (fake_backend_t*) ctx;
    fake->enter_count++;
    return 0xA55Au;
}

static void fake_critical_exit(void* ctx, uint32_t state)
{
    fake_backend_t* fake = (fake_backend_t*) ctx;
    fake->exit_count++;
    fake->last_exit_state = state;
}

static int fake_start(void* ctx)
{
    fake_backend_t* fake = (fake_backend_t*) ctx;
    fake->start_count++;
    return fake->start_result;
}

static shell_port_backend_t make_backend(void)
{
    shell_port_backend_t backend = {
        .ctx = &s_fake,
        .write = fake_write,
        .critical_enter = fake_critical_enter,
        .critical_exit = fake_critical_exit,
        .start = fake_start,
    };
    return backend;
}

void setUp(void)
{
    memset(&s_fake, 0, sizeof(s_fake));
}

void tearDown(void)
{
}

void test_port_init_rejects_missing_backend_or_write(void)
{
    shell_port_backend_t backend = make_backend();
    backend.write = NULL;

    TEST_ASSERT_EQUAL_INT(-1, shell_port_init(NULL));
    TEST_ASSERT_EQUAL_INT(-1, shell_port_init(&backend));
}

void test_port_init_starts_backend_and_routes_write(void)
{
    shell_port_backend_t backend = make_backend();

    TEST_ASSERT_EQUAL_INT(0, shell_port_init(&backend));
    TEST_ASSERT_EQUAL_UINT16(1, s_fake.start_count);

    shell_print(shell_port_get_shell(), "OK");

    TEST_ASSERT_EQUAL_STRING("OK", s_fake.tx);
}

void test_port_init_returns_start_error(void)
{
    shell_port_backend_t backend = make_backend();
    s_fake.start_result = -7;

    TEST_ASSERT_EQUAL_INT(-7, shell_port_init(&backend));
}

void test_port_forwards_critical_hooks_to_log_queue(void)
{
    shell_port_backend_t backend = make_backend();

    TEST_ASSERT_EQUAL_INT(0, shell_port_init(&backend));
    TEST_ASSERT_EQUAL_INT(2, shell_log_isr(shell_port_get_shell(), (const uint8_t*) "AB", 2));

    TEST_ASSERT_EQUAL_UINT16(1, s_fake.enter_count);
    TEST_ASSERT_EQUAL_UINT16(1, s_fake.exit_count);
    TEST_ASSERT_EQUAL_UINT32(0xA55Au, s_fake.last_exit_state);
}

void test_port_rx_from_isr_feeds_shell_rx_buffer(void)
{
    shell_port_backend_t backend = make_backend();
    char                 rx[2] = {0};

    TEST_ASSERT_EQUAL_INT(0, shell_port_init(&backend));
    shell_port_rx_from_isr((uint8_t) 'A');

    TEST_ASSERT_EQUAL_INT(1, shell_rx_read(shell_port_get_shell(), rx, sizeof(rx)));
    TEST_ASSERT_EQUAL_CHAR('A', rx[0]);
}

void test_port_rx_buf_from_isr_feeds_shell_rx_buffer(void)
{
    shell_port_backend_t backend = make_backend();
    const uint8_t        data[] = {'A', 'B', 'C'};
    char                 rx[4] = {0};

    TEST_ASSERT_EQUAL_INT(0, shell_port_init(&backend));
    shell_port_rx_buf_from_isr(data, sizeof(data));

    TEST_ASSERT_EQUAL_INT(3, shell_rx_read(shell_port_get_shell(), rx, sizeof(rx)));
    TEST_ASSERT_EQUAL_MEMORY(data, rx, sizeof(data));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_port_init_rejects_missing_backend_or_write);
    RUN_TEST(test_port_init_starts_backend_and_routes_write);
    RUN_TEST(test_port_init_returns_start_error);
    RUN_TEST(test_port_forwards_critical_hooks_to_log_queue);
    RUN_TEST(test_port_rx_from_isr_feeds_shell_rx_buffer);
    RUN_TEST(test_port_rx_buf_from_isr_feeds_shell_rx_buffer);

    return UNITY_END();
}
