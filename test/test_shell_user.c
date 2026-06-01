/**
 * @file    test_shell_user.c
 * @brief   Project user table and verifier tests
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

#include "test_shell_mock.h"
#include "unity.h"

#include "../shell.c"
#include "../shell_log.c"
#include "../shell_user.c"

shell_cmd_t s_test_cmds[] = {
    {.name = "ok", .desc = "always ok", .func = test_cmd_ok, .permission = 0, .comp_list = NULL},
};
uint16_t g_test_cmd_count = sizeof(s_test_cmds) / sizeof(s_test_cmds[0]);

const shell_var_t g_test_vars[] = {
    {.name = "dummy", .ptr = NULL, .type = SHELL_VAR_INT, .is_readonly = 0},
};
uint16_t g_test_var_count = 1;

static shell_t s_sh;

void setUp(void)
{
    mock_write_reset();
    shell_config_t cfg = {.write = mock_write};
    shell_init(&s_sh, &cfg);
    s_sh.is_inited = 1;
}

void tearDown(void)
{
}

void test_shell_user_init_sets_users_and_password_verifier(void)
{
    shell_user_init(&s_sh);

    TEST_ASSERT_NOT_NULL(s_sh.users);
    TEST_ASSERT_EQUAL_UINT8(3, s_sh.user_cnt);
    TEST_ASSERT_NOT_NULL(s_sh.password_verify);
}

void test_shell_user_password_verify_accepts_configured_credentials(void)
{
    shell_user_init(&s_sh);

    TEST_ASSERT_EQUAL_INT(0, shell_login(&s_sh, "root", "123456"));
    TEST_ASSERT_EQUAL_STRING("root", s_sh.cur_user->name);

    shell_logout(&s_sh);
    TEST_ASSERT_EQUAL_INT(0, shell_login(&s_sh, "admin", "admin"));
    TEST_ASSERT_EQUAL_STRING("admin", s_sh.cur_user->name);

    shell_logout(&s_sh);
    TEST_ASSERT_EQUAL_INT(0, shell_login(&s_sh, "guest", ""));
    TEST_ASSERT_EQUAL_STRING("guest", s_sh.cur_user->name);
}

void test_shell_user_password_verify_rejects_wrong_password(void)
{
    shell_user_init(&s_sh);

    TEST_ASSERT_EQUAL_INT(-2, shell_login(&s_sh, "admin", "bad"));
    TEST_ASSERT_NULL(s_sh.cur_user);
    TEST_ASSERT_EQUAL_UINT8(0, s_sh.is_checked);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_shell_user_init_sets_users_and_password_verifier);
    RUN_TEST(test_shell_user_password_verify_accepts_configured_credentials);
    RUN_TEST(test_shell_user_password_verify_rejects_wrong_password);

    return UNITY_END();
}
