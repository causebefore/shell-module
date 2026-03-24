/**
 * @file    shell_port.c
 * @brief   Shell移植层 - UART3 中断收发 + LWRB环形缓冲区
 *
 * 使用 LWRB 库实现收发缓冲区
 * 适用于无DMA的串口
 */

#include "shell_port.h"

#include "lwrb.h"
#include "usart.h"

#include <string.h>

/* Shell实例 */
shell_t g_shell_usart3;

/* ==================== LWRB 环形缓冲区 ==================== */

#define PORT_TX_BUF_SIZE 1024
#define PORT_RX_BUF_SIZE 64

static lwrb_t           s_tx_rb;                    /* 发送环形缓冲区 */
static lwrb_t           s_rx_rb;                    /* 接收环形缓冲区 */
static uint8_t          s_tx_buf[PORT_TX_BUF_SIZE]; /* 发送缓冲区数据 */
static uint8_t          s_rx_buf[PORT_RX_BUF_SIZE]; /* 接收缓冲区数据 */
static volatile uint8_t s_tx_busy = 0;              /* 发送忙标志 */

/* ==================== IO实现 ==================== */

/**
 * @brief 启动发送（从缓冲区取数据发送）
 */
static void shell_start_tx(void)
{
    if (s_tx_busy)
    {
        return; /* 正在发送中 */
    }

    /* 检查是否有数据要发送 */
    if (lwrb_get_full(&s_tx_rb) > 0)
    {
        s_tx_busy = 1;
        /* 使能TXE中断，让中断处理函数来发送数据 */
        __HAL_UART_ENABLE_IT(&huart3, UART_IT_TXE);
    }
}

/**
 * @brief Shell写函数 - 写入发送缓冲区
 */
static void shell_write_impl(const char* data, uint16_t len)
{
    lwrb_write(&s_tx_rb, data, len);
    shell_start_tx();
}

/**
 * @brief Shell读函数 - 从接收缓冲区读取
 */
static int shell_read_impl(char* data, uint16_t len)
{
    return (int) lwrb_read(&s_rx_rb, data, len);
}

/**
 * @brief UART3 中断处理 - 在 stm32f4xx_it.c 的 USART3_IRQHandler 中调用
 * @note 需要在 USART3_IRQHandler 中添加: shell_uart3_irq_handler();
 */
void shell_uart3_irq_handler(void)
{
    /* 接收中断 */
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
    {
        uint8_t ch = (uint8_t) (huart3.Instance->DR & 0xFF);
        lwrb_write(&s_rx_rb, &ch, 1); /* 写入接收缓冲区 */
    }

    /* 发送空中断 */
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TXE) && __HAL_UART_GET_IT_SOURCE(&huart3, UART_IT_TXE))
    {
        uint8_t ch;
        if (lwrb_read(&s_tx_rb, &ch, 1) == 1)
        {
            huart3.Instance->DR = ch; /* 发送下一个字节 */
        }
        else
        {
            __HAL_UART_DISABLE_IT(&huart3, UART_IT_TXE); /* 禁用发送空中断 */
            s_tx_busy = 0;
        }
    }
}

/* ==================== 用户定义 ==================== */

#if SHELL_USING_AUTH

    #if SHELL_USING_HASH_PWD
        /*
         * 密码哈希值生成工具:
         * 运行 shell_hash("your_password") 获取哈希值
         * 然后将哈希值填入下方 password 字段
         */
        #define HASH_ROOT  0x7DD1705AUL /* hash("123456") */
        #define HASH_ADMIN 0x0F12FC8EUL /* hash("admin")  */
        #define HASH_NONE  0x00000000UL /* 无密码 */

static const shell_user_t s_shell_users[] = {
    {"root",  (const char*) (uintptr_t) HASH_ROOT,  SHELL_PERM_ROOT }, /* 超级用户 */
    {"admin", (const char*) (uintptr_t) HASH_ADMIN, SHELL_PERM_ADMIN}, /* 管理员 */
    {"guest", (const char*) (uintptr_t) HASH_NONE,  SHELL_PERM_USER }, /* 访客(无密码) */
};
    #else
/* 明文密码方式 (不推荐) */
static const shell_user_t s_shell_users[] = {
    {"root",  "123456", SHELL_PERM_ROOT }, /* 超级用户 */
    {"admin", "admin",  SHELL_PERM_ADMIN}, /* 管理员 */
    {"guest", "",       SHELL_PERM_USER }, /* 访客(无密码) */
};
    #endif

    #define USER_COUNT (sizeof(s_shell_users) / sizeof(s_shell_users[0]))
#endif

/* ==================== 用户命令 ==================== */

static int cmd_test(int argc, char* argv[])
{
    shell_print(g_shell, "Test OK\r\n");
    return 0;
}

/* 需要管理员权限的命令示例 */
static int cmd_reboot(int argc, char* argv[])
{
    shell_print(g_shell, "System rebooting...\r\n");
    /* HAL_NVIC_SystemReset(); */
    return 0;
}

/* 带参数补全的命令示例 */
static int cmd_mode(int argc, char* argv[])
{
    if (argc < 2)
    {
        shell_print(g_shell, "Usage: mode <speed|angle|torque>\r\n");
        return -1;
    }
    shell_printf(g_shell, "Mode set to: %s\r\n", argv[1]);
    return 0;
}

/* 补全列表 (必须以 NULL 结尾) */
static const char* s_mode_opts[] = {"speed", "angle", "torque", NULL};

/* ==================== 宏注册命令 ==================== */

/* 内置命令 */
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

/* 用户命令 */
SHELL_EXPORT_CMD(test, "Test command", cmd_test, SHELL_PERM_USER);                   /* 需要登录 */
SHELL_EXPORT_CMD(reboot, "System reboot", cmd_reboot, SHELL_PERM_ADMIN);             /* 需要管理员 */
SHELL_EXPORT_CMD_LIST(mode, "Set FOC mode", cmd_mode, SHELL_PERM_NONE, s_mode_opts); /* 带参数补全 */

/* ==================== 示例变量导出 ==================== */
#if SHELL_USING_VAR
static int         s_test_int   = 100;
static uint32_t    s_test_uint  = 0x12345678;
static float       s_test_float = 3.14f;
static uint8_t     s_test_bool  = 1;
static const char* s_version    = "1.0.0";

SHELL_EXPORT_VAR(test_int, &s_test_int, SHELL_VAR_INT);
SHELL_EXPORT_VAR(test_uint, &s_test_uint, SHELL_VAR_UINT);
SHELL_EXPORT_VAR(test_float, &s_test_float, SHELL_VAR_FLOAT);
SHELL_EXPORT_VAR(test_bool, &s_test_bool, SHELL_VAR_BOOL);
SHELL_EXPORT_VAR_RO(version, &s_version, SHELL_VAR_STRING);
#endif

/* ==================== 初始化 ==================== */

void my_shell_init(void)
{
    /* 初始化LWRB环形缓冲区 */
    lwrb_init(&s_tx_rb, s_tx_buf, sizeof(s_tx_buf));
    lwrb_init(&s_rx_rb, s_rx_buf, sizeof(s_rx_buf));

    /* 初始化shell (使用宏导出命令) */
    shell_init_export(&g_shell_usart3, shell_write_impl, shell_read_impl);

#if SHELL_USING_AUTH
    /* 设置用户列表 */
    shell_set_users(&g_shell_usart3, s_shell_users, USER_COUNT);
#endif

    /* 启用UART3接收中断 */
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
}

void my_shell_task(void)
{
    shell_task(&g_shell_usart3);
}
