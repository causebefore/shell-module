/**
 * @file    shell_port.c
 * @brief   Shell移植层 - UART1 DMA发送 + DMA空闲接收
 *
 * 方式1: DMA空闲接收 (当前使用)
 *   - 使用 HAL_UARTEx_ReceiveToIdle_DMA 进行批量接收
 *   - 适合高波特率、大数据量场景
 *
 * 方式2: 中断接收 + 内置环形缓冲区 (见文件末尾示例)
 *   - 在UART中断中调用 shell_rx_push()
 *   - shell_init 时 read 参数传 NULL
 *   - 适合简单场景，无需DMA
 */

#include "shell_port.h"

#include "usart.h"

#include <string.h>

/* Shell实例 */
shell_t g_shell_usart1;

/* DMA接收缓冲区 (独立于shell内置缓冲区) */
#define PORT_RX_BUF_SIZE 64
static uint8_t           s_rx_buf[PORT_RX_BUF_SIZE];
static volatile uint16_t s_rx_len  = 0;
static volatile uint8_t  s_rx_flag = 0;

/* ==================== IO实现 ==================== */

static void shell_write_impl(const char* data, uint16_t len)
{
    UART_DMA_TxBuffer_Write(&huart1, (const uint8_t*) data, len);
}

static int shell_read_impl(char* data, uint16_t len)
{
    if (s_rx_flag && s_rx_len > 0)
    {
        uint16_t copy_len = (s_rx_len < len) ? s_rx_len : len;
        memcpy(data, s_rx_buf, copy_len);
        s_rx_flag = 0;

        /* 重新启动DMA接收 */
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_rx_buf, PORT_RX_BUF_SIZE);
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT); /* 禁用半传输中断 */

        return copy_len;
    }
    return 0;
}

/**
 * @brief HAL UART空闲回调 - 在 stm32f4xx_it.c 中被 HAL_UART_IRQHandler 调用
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        s_rx_len  = Size;
        s_rx_flag = 1;
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
    /* 初始化shell (使用宏导出命令) */
    shell_init_export(&g_shell_usart1, shell_write_impl, shell_read_impl);

#if SHELL_USING_AUTH
    /* 设置用户列表 */
    shell_set_users(&g_shell_usart1, s_shell_users, USER_COUNT);
#endif

    /* 启动DMA接收 (空闲中断模式) */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_rx_buf, PORT_RX_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT); /* 禁用半传输中断 */
}

void my_shell_task(void)
{
    shell_task(&g_shell_usart1);
}

/* ==================== 方式2: 中断接收示例 (无需DMA) ==================== */
#if 0 /* 启用此方式时改为 1, 并禁用上方DMA相关代码 */

/**
 * 使用内置环形缓冲区的初始化方式:
 * 1. shell_init 时 read 参数传 NULL
 * 2. 在 UART 接收中断中调用 shell_rx_push()
 */

void my_shell_init_simple(void)
{
    /* read=NULL 时 shell_task 自动使用内置环形缓冲区 */
    shell_init_export(&g_shell_usart1, shell_write_impl, NULL);

    #if SHELL_USING_AUTH
    shell_set_users(&g_shell_usart1, s_shell_users, USER_COUNT);
    #endif

    /* 启用UART接收中断 */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
}

/**
 * UART 接收中断处理 (在 stm32f4xx_it.c 的 USART1_IRQHandler 中调用)
 */
void shell_uart_irq_handler(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE))
    {
        uint8_t ch = (uint8_t)(huart1.Instance->DR & 0xFF);
        shell_rx_push(&g_shell_usart1, ch);  /* 写入内置环形缓冲区 */
    }
}

#endif /* 方式2示例 */
