/**
 * @file    test_shell_mock.h
 * @brief   Shell模块测试用 mock/桩函数声明
 *
 * 提供 write/read 回调的 mock 实现，用于捕获输出和注入输入。
 */
#ifndef TEST_SHELL_MOCK_H
#define TEST_SHELL_MOCK_H

#include <stdint.h>

/* ==================== 输出捕获 ==================== */

#define MOCK_TX_BUF_SIZE 2048

/* 重置输出缓冲区 */
void mock_write_reset(void);

/* mock write 回调，签名与 shell_t.write 一致 */
void mock_write(const char* data, uint16_t len);

/* 获取已捕获的输出内容 (null终止) */
const char* mock_write_get(void);

/* 获取已捕获的输出长度 */
uint16_t mock_write_len(void);

/* 检查输出是否包含指定子串 */
int mock_write_contains(const char* substr);

/* ==================== 输入注入 ==================== */

#define MOCK_RX_BUF_SIZE 256

/* 设置待读取的输入数据 */
void mock_read_set(const char* data, uint16_t len);

/* mock read 回调，签名与 shell_t.read 一致 */
int mock_read(char* buf, uint16_t max_len);

/* ==================== 测试命令 ==================== */

/* 简单测试命令，返回 0 */
int test_cmd_ok(int argc, char* argv[]);

/* 返回指定错误码的测试命令 (argv[1] 为错误码字符串) */
int test_cmd_fail(int argc, char* argv[]);

/* 记录最后一次调用信息 */
int test_cmd_record(int argc, char* argv[]);

/* 获取 record 命令最后调用的 argc */
int test_cmd_record_get_argc(void);

/* 获取 record 命令最后调用的 argv[n]，越界返回 NULL */
const char* test_cmd_record_get_argv(int n);

#endif /* TEST_SHELL_MOCK_H */
