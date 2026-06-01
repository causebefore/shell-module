/**
 * @file    shell_port_stdperiph.c
 * @brief   STM32 标准外设库 Shell 移植示例
 *
 * 本文件演示如何将 Shell 模块移植到 STM32 标准外设库环境。
 * 使用 USART1 作为 Shell 通信接口。
 *
 * 硬件连接:
 *   PA9  - USART1_TX
 *   PA10 - USART1_RX
 *
 * 使用方法:
 *   1. 配置 USART1 (115200-8-N-1)
 *   2. 开启 USART1 中断
 *   3. 将本文件添加到工程
 *   4. 在 main() 中调用 shell_port_init()
 *   5. 在主循环中调用 shell_port_task()
 */

#include "shell.h"
#include "shell_ansi.h"

#include "stm32f10x.h"  /* 根据实际芯片系列修改头文件 */
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* ==================== 硬件配置 ==================== */

#define SHELL_USART           USART1
#define SHELL_USART_CLK       RCC_APB2Periph_USART1
#define SHELL_USART_GPIO      GPIOA
#define SHELL_USART_GPIO_CLK  RCC_APB2Periph_GPIOA
#define SHELL_USART_TX_PIN    GPIO_Pin_9
#define SHELL_USART_RX_PIN    GPIO_Pin_10
#define SHELL_USART_IRQn      USART1_IRQn
#define SHELL_USART_IRQHandler USART1_IRQHandler

/* ==================== Shell 实例 ==================== */

static shell_t s_shell;

/* ==================== 写回调 ==================== */

/**
 * @brief 串口发送回调 (阻塞方式)
 */
static void shell_write(const char* data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        while (USART_GetFlagStatus(SHELL_USART, USART_FLAG_TXE) == RESET);
        USART_SendData(SHELL_USART, data[i]);
    }
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
        if (USART_GetFlagStatus(SHELL_USART, USART_FLAG_RXNE) == SET)
        {
            buf[cnt++] = (char) USART_ReceiveData(SHELL_USART);
        }
        else
        {
            break;
        }
    }
    return cnt;
}

/* ==================== 中断接收方式 (推荐) ==================== */

/**
 * @brief USART1 中断处理函数
 * @note  在 stm32f10x_it.c 中实现，或直接在此文件实现
 */
void SHELL_USART_IRQHandler(void)
{
    if (USART_GetITStatus(SHELL_USART, USART_IT_RXNE) == SET)
    {
        uint8_t ch = (uint8_t) USART_ReceiveData(SHELL_USART);
        shell_rx_push(&s_shell, ch);
        USART_ClearITPendingBit(SHELL_USART, USART_IT_RXNE);
    }
}

/* ==================== 硬件初始化 ==================== */

/**
 * @brief USART1 GPIO 和时钟配置
 */
static void shell_uart_init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    /* 使能时钟 */
    RCC_APB2PeriphClockCmd(SHELL_USART_GPIO_CLK | SHELL_USART_CLK | RCC_APB2Periph_AFIO, ENABLE);

    /* 配置 TX 引脚 (复用推挽输出) */
    GPIO_InitStructure.GPIO_Pin   = SHELL_USART_TX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SHELL_USART_GPIO, &GPIO_InitStructure);

    /* 配置 RX 引脚 (浮空输入) */
    GPIO_InitStructure.GPIO_Pin   = SHELL_USART_RX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SHELL_USART_GPIO, &GPIO_InitStructure);

    /* 配置 USART 参数 */
    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(SHELL_USART, &USART_InitStructure);

    /* 配置 NVIC 中断 */
    NVIC_InitStructure.NVIC_IRQChannel                   = SHELL_USART_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 使能接收中断 */
    USART_ITConfig(SHELL_USART, USART_IT_RXNE, ENABLE);

    /* 使能 USART */
    USART_Cmd(SHELL_USART, ENABLE);
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
    (void) user;
    (void) input_password;

    /* 密码策略必须由项目接管，例如:
     * - 哈希比较 (SHA-256 等)
     * - 外部存储查询 (EEPROM、Flash)
     * - 网络认证 (LDAP、RADIUS)
     */

    return -1; /* 默认拒绝，替换为项目自己的验证逻辑 */
}

/* 用户列表 */
static const shell_user_t s_users[] = {
    {.name = "admin", .permission = SHELL_PERM_ADMIN},
    {.name = "user",  .permission = SHELL_PERM_USER },
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
    shell_printf(g_shell, "  Clock:    %lu Hz\r\n", SystemCoreClock);
    shell_printf(g_shell, "  Uptime:   %lu ms\r\n", (unsigned long) SysTick->VAL);

    return 0;
}

/* 命令导出 (需要链接脚本支持) */
// SHELL_EXPORT_CMD(sysinfo, "Show system info", cmd_sysinfo, SHELL_PERM_NONE);

/* ==================== 初始化与任务 ==================== */

/**
 * @brief Shell 端口初始化
 * @note  在 main() 中调用，应在系统时钟配置之后
 */
void shell_port_init(void)
{
    /* 初始化 USART 硬件 */
    shell_uart_init();

    shell_config_t cfg = {
        .write = shell_write,
#if SHELL_USING_AUTH
        .password_verify = custom_password_verify,
#endif
    };

    shell_init(&s_shell, &cfg);

#if SHELL_USING_AUTH
    shell_set_users(&s_shell, s_users, sizeof(s_users) / sizeof(s_users[0]));
#endif

    /* 方式1: 使用 read 回调 (查询方式) */
    // s_shell.read = shell_read;

    /* 方式2: 使用中断接收 (推荐) */
    s_shell.read = NULL;  /* 不设置 read，使用环形缓冲区 */

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

/* ==================== 使用示例 ==================== */

/*
int main(void)
{
    // 系统初始化
    SystemInit();
    SysTick_Config(SystemCoreClock / 1000);  // 1ms 系统时钟

    // Shell 初始化
    shell_port_init();

    // 主循环
    while (1)
    {
        shell_port_task();

        // 其他任务...
    }
}
*/
