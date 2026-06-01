/**
 * @file    test_shell_auth.c
 * @brief   密码验证回调 Unity 单元测试
 *
 * 覆盖范围:
 *   - shell_set_password_verify  基本功能 (设置自定义回调)
 *   - shell_set_password_verify  NULL参数 (禁用登录验证)
 *   - 自定义回调在 shell_login 中的调用
 *
 * 构建方式 (主机端GCC):
 *   cd test
 *   gcc -Wall -Wextra -I. -Iunity/src -I.. test_shell_auth.c test_shell_mock.c unity/src/unity.c -o test_shell_auth
 *   ./test_shell_auth
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

/* ==================== 测试用用户表 ==================== */

static const shell_user_t s_test_users[] = {
    {.name = "admin", .permission = SHELL_PERM_ADMIN},
    {.name = "user",  .permission = SHELL_PERM_USER },
};

#define TEST_USER_COUNT (sizeof(s_test_users) / sizeof(s_test_users[0]))

/* ==================== 自定义验证回调 ==================== */

static int s_verify_called = 0;

static int custom_verify_always_pass(const shell_user_t* user, const char* input_password)
{
    (void) user;
    (void) input_password;
    s_verify_called++;
    return 0; /* 总是通过 */
}

static int custom_verify_always_fail(const shell_user_t* user, const char* input_password)
{
    (void) user;
    (void) input_password;
    s_verify_called++;
    return -1; /* 总是失败 */
}

/* ==================== 测试夹具 ==================== */

static shell_t s_sh;

static void setup_shell(void)
{
    mock_write_reset();
    shell_config_t cfg = {.write = mock_write};
    shell_init(&s_sh, &cfg);
    s_sh.read      = mock_read;
    s_sh.is_inited = 1;
    shell_set_users(&s_sh, s_test_users, TEST_USER_COUNT);
    s_verify_called = 0;
}

void setUp(void)
{
    setup_shell();
}

void tearDown(void)
{
}

/* =================================================================
 * 测试组1: shell_set_password_verify 基本功能
 * ================================================================= */

void test_set_password_verify_basic(void)
{
    shell_set_password_verify(&s_sh, custom_verify_always_pass);
    TEST_ASSERT_EQUAL_PTR(custom_verify_always_pass, s_sh.password_verify);
}

void test_set_password_verify_used_on_login(void)
{
    shell_set_password_verify(&s_sh, custom_verify_always_pass);
    /* 使用自定义回调登录，应该通过 */
    int ret = shell_login(&s_sh, "admin", "any_password");
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(1, s_verify_called);
    TEST_ASSERT_NOT_NULL(s_sh.cur_user);
    TEST_ASSERT_EQUAL_STRING("admin", s_sh.cur_user->name);
}

void test_set_password_verify_fail_callback(void)
{
    shell_set_password_verify(&s_sh, custom_verify_always_fail);
    /* 使用自定义回调登录，应该失败 */
    int ret = shell_login(&s_sh, "admin", "any_password");
    TEST_ASSERT_EQUAL_INT(-2, ret);
    TEST_ASSERT_EQUAL_INT(1, s_verify_called);
    TEST_ASSERT_NULL(s_sh.cur_user);
}

void test_set_password_verify_called_per_login(void)
{
    shell_set_password_verify(&s_sh, custom_verify_always_pass);
    shell_login(&s_sh, "admin", "pass1");
    shell_login(&s_sh, "user", "pass2");
    TEST_ASSERT_EQUAL_INT(2, s_verify_called);
}

/* =================================================================
 * 测试组2: shell_set_password_verify NULL参数
 * ================================================================= */

void test_set_password_verify_null_clears_verifier(void)
{
    /* 先设置自定义回调 */
    shell_set_password_verify(&s_sh, custom_verify_always_pass);
    TEST_ASSERT_EQUAL_PTR(custom_verify_always_pass, s_sh.password_verify);

    /* 设置NULL会清除 verifier，后续登录不再有内置兜底 */
    shell_set_password_verify(&s_sh, NULL);
    TEST_ASSERT_NULL(s_sh.password_verify);
}

void test_set_password_verify_null_shell(void)
{
    /* NULL shell 不应崩溃 */
    shell_set_password_verify(NULL, custom_verify_always_pass);
}

void test_set_password_verify_null_both(void)
{
    /* 都为NULL不应崩溃 */
    shell_set_password_verify(NULL, NULL);
}

void test_login_without_password_verify_fails(void)
{
    /* 设置自定义回调再清除 */
    shell_set_password_verify(&s_sh, custom_verify_always_pass);
    shell_set_password_verify(&s_sh, NULL);

    /* 没有 password_verify 时不允许使用任何内置密码逻辑登录 */
    int ret = shell_login(&s_sh, "user", "");
    TEST_ASSERT_EQUAL_INT(-2, ret);
    TEST_ASSERT_NULL(s_sh.cur_user);
    TEST_ASSERT_EQUAL_UINT8(0, s_sh.is_checked);
    /* 自定义回调不应被调用 */
    TEST_ASSERT_EQUAL_INT(0, s_verify_called);
}

/* =================================================================
 * 测试组3: 通过配置结构体设置回调
 * ================================================================= */

void test_init_with_password_verify(void)
{
    shell_t sh;
    shell_config_t cfg = {
        .write           = mock_write,
        .password_verify  = custom_verify_always_pass,
    };
    shell_init(&sh, &cfg);
    TEST_ASSERT_EQUAL_PTR(custom_verify_always_pass, sh.password_verify);
}

/* =================================================================
 * 主入口
 * ================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* shell_set_password_verify 基本功能 */
    RUN_TEST(test_set_password_verify_basic);
    RUN_TEST(test_set_password_verify_used_on_login);
    RUN_TEST(test_set_password_verify_fail_callback);
    RUN_TEST(test_set_password_verify_called_per_login);

    /* shell_set_password_verify NULL参数 */
    RUN_TEST(test_set_password_verify_null_clears_verifier);
    RUN_TEST(test_set_password_verify_null_shell);
    RUN_TEST(test_set_password_verify_null_both);
    RUN_TEST(test_login_without_password_verify_fails);

    /* 通过配置结构体设置 */
    RUN_TEST(test_init_with_password_verify);

    return UNITY_END();
}
