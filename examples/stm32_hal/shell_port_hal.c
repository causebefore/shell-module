/**
 * @file    shell_port_hal.c
 * @brief   STM32 HAL 库 Shell 移植示例
 *
 * 本文件演示如何将 Shell 模块移植到 STM32 HAL 库环境。
 * 使用 USART1 作为 Shell 通信接口。
 *
 * 硬件连接:
 *   PA9  - USART1_TX
 *   PA10 - USART1_RX
 *
 * 使用方法:
 *   1. 在 CubeMX 中配置 USART1 (115200-8-N-1)
 *   2. 开启 USART1 全局中断
 *   3. 将本文件添加到工程
 *   4. 在 main.c 中调用 shell_port_init()
 *   5. 在主循环中调用 shell_port_task()
 */

#include "shell.h"
#include "shell_ansi.h"

#include "stm32f1xx_hal.h"  /* 根据实际芯片系列修改头文件 */
#include <string.h>

/* ==================== 硬件配置 ==================== */

extern UART_HandleTypeDef huart1;  /* CubeMX 生成的句柄 */

/* ==================== Shell 实例 ==================== */

static shell_t s_shell;

/* ==================== 写回调 ==================== */

/**
 * @brief 串口发送回调 (阻塞方式)
 */
static void shell_write(const char* data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t*) data, len, HAL_MAX_DELAY);
}

/* ==================== 读回调 (可选) ==================== */

/**
 * @brief 串口读取回调 (查询方式)
 * @note  推荐使用中断 + 环形缓冲区方式，效率更高
 */
static int shell_read(char* buf, uint16_t max_len)
{
    uint16_t cnt = 0;
    while (cnt < max_len)
    {
        uint8_t ch;
        if (HAL_UART_Receive(&huart1, &ch, 1, 0) == HAL_OK)
        {
            buf[cnt++] = ch;
        }
        else
        {
            break;
        }
    }
    return cnt;
}

/* ==================== 中断接收方式 (推荐) ==================== */

static uint8_t s_rx_byte;  /* 单字节接收缓冲 */

/**
 * @brief USART 接收完成回调 (在 stm32f1xx_it.c 中调用)
 * @note  需要在 HAL_UART_RxCpltCallback 中调用此函数
 */
void shell_uart_rx_callback(void)
{
    /* 将接收到的字节推送到 Shell 环形缓冲区 */
    shell_rx_push(&s_shell, s_rx_byte);

    /* 重新启动中断接收 */
    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
}

/**
 * @brief HAL UART 接收完成回调 (CubeMX 回调)
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart == &huart1)
    {
        shell_uart_rx_callback();
    }
}

/* ==================== 日志输出 ==================== */

/**
 * @brief 带时间戳的日志输出 (可在中断中调用)
 * @note  使用 ISR 安全日志队列，由主循环 drain 输出
 */
void shell_log_printf(const char* fmt, ...)
{
    char    buf[128];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len > 0)
    {
        if (len > (int) sizeof(buf) - 1)
        {
            len = sizeof(buf) - 1;
        }
        shell_log_text_isr(&s_shell, buf);
    }
}

/* ==================== 密码验证 (可选) ==================== */

#if SHELL_USING_AUTH

/**
 * @brief 自定义密码验证回调示例
 * @note  可在此处实现更安全的密码验证逻辑
 */
static int custom_password_verify(const shell_user_t* user, const char* input_password)
{
    /* 示例: 使用 strcmp 进行明文比较 (仅用于调试) */
    (void) user;
    (void) input_password;

    /* 实际应用中可使用:
     * - 哈希比较 (SHA-256 等)
     * - 外部存储查询 (EEPROM、Flash)
     * - 网络认证 (LDAP、RADIUS)
     */

    return -1; /* 默认拒绝，使用默认验证 */
}

/* 用户列表 */
static const shell_user_t s_users[] = {
    {.name = "admin", .password = (const char*) (uintptr_t) 0xB888BBCFU, .permission = SHELL_PERM_ADMIN},
    {.name = "user",  .password = (const char*) (uintptr_t) 0x0U,        .permission = SHELL_PERM_USER },
};

#endif /* SHELL_USING_AUTH */

/* ==================== 导出命令示例 ==================== */

/**
 * @brief 系统信息命令
 */
static int cmd_sysinfo(int argc, char* argv[])
{
    (void) argc;
    (void) argv;

    shell_printf_color(g_shell, ANSI_COLOR_CYAN, "System Information:\r\n");
    shell_printf(g_shell, "  MCU:      STM32F103\r\n");
    shell_printf(g_shell, "  HAL:      %s\r\n", HAL_GetHalVersion());
    shell_printf(g_shell, "  Uptime:   %lu ms\r\n", HAL_GetTick());
    shell_printf(g_shell, " "  "Heap:     %lu bytes free\r\n",
                 (unsigned long) HAL_GetFreeHeapSize());

    return 0;
}

/* 命令导出 (需要链接脚本支持) */
// SHELL_EXPORT_CMD(sysinfo, "Show system info", cmd_sysinfo, SHELL_PERM_NONE);

/* ==================== 初始化与任务 ==================== */

/**
 * @brief Shell 端口初始化
 * @note  在 main() 中调用，应在 HAL_Init() 和系统时钟配置之后
 */
void shell_port_init(void)
{
    shell_config_t cfg = {
        .write = shell_write,
#if SHELL_USING_AUTH
        .password_verify = NULL,  /* 使用默认验证，或设置 custom_password_verify */
#endif
    };

    shell_init(&s_shell, &cfg);

#if SHELL_USING_AUTH
    shell_set_users(&s_shell, s_users, sizeof(s_users) / sizeof(s_users[0]));
#endif

    /* 方式1: 使用 read 回调 (查询方式) */
    s_shell.read = shell_read;

    /* 方式2: 使用中断接收 (推荐) */
    // s_shell.read = NULL;  /* 不设置 read，使用环形缓冲区 */
    // HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);

    /* 可选: 设置自定义密码验证 */
    // shell_set_password_verify(&s_shell, custom_password_verify);
}

/**
 * @brief Shell 端口任务
 * @note  在主循环中调用
 */
void shell_port_task(void)
{
    shell_task(&s_shell);
}

/**
 * @brief 获取 Shell 实例指针
 * @return Shell 实例指针
 */
shell_t* shell_port_get_instance(void)
{
    return &s_shell;
}

/* ==================== 内存信息 (可选) ==================== */

/**
 * @brief 获取空闲堆大小 (需要实现)
 */
__weak unsigned long HAL_GetFreeHeapSize(void)
{
    /* 需要根据实际内存管理方案实现 */
    /* 示例: 使用 FreeRTOS 的 xPortGetFreeHeapSize() */
    /* 或使用 newlib 的 mallinfo() */
    return 0;
}

/**
 * @brief 获取 HAL 版本字符串 (需要实现)
 */
__weak const char* HAL_GetHalVersion(void)
{
    /* 根据实际 HAL 版本返回 */
    return "1.x.x";
}
