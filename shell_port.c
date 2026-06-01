/**
 * @file    shell_port.c
 * @brief   Shell移植层 - 简化版
 *
 * 仅包含硬件IO和中断处理，约40行代码
 */

#include "shell.h"
#include "main.h"  /* STM32 HAL: USART3, USART_SR_TXE, USART_CR1_RXNEIE 等 */

#ifndef SHELL_PORT_TX_TIMEOUT
#define SHELL_PORT_TX_TIMEOUT 1000000u
#endif

/* Shell实例 */
static shell_t s_shell;

/* ==================== 硬件IO层 ==================== */

static uint32_t shell_port_critical_enter(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void shell_port_critical_exit(uint32_t state)
{
    __set_PRIMASK(state);
}

static int uart_wait_flag(uint32_t flag)
{
    uint32_t timeout = SHELL_PORT_TX_TIMEOUT;
    while ((USART3->SR & flag) == 0u)
    {
        if (timeout == 0u)
        {
            return 0;
        }
        timeout--;
    }
    return 1;
}

/**
 * @brief Shell写函数 - 阻塞发送
 * @note  仅允许在主循环/任务上下文调用，不要在ISR中直接打印。
 */
static void uart_write(const char* data, uint16_t len)
{
    if (!data)
    {
        return;
    }

    /* 阻塞发送，简单可靠 */
    for (uint16_t i = 0; i < len; i++)
    {
        if (!uart_wait_flag(USART_SR_TXE))
        {
            return;
        }
        USART3->DR = (uint8_t) data[i];
    }
    (void) uart_wait_flag(USART_SR_TC);
}

/* ==================== 中断处理 ==================== */

/**
 * @brief UART3中断处理 - 在USART3_IRQHandler中调用
 */
void shell_uart3_irq_handler(void)
{
    if (USART3->SR & USART_SR_RXNE)
    {
        uint8_t ch = (uint8_t)(USART3->DR & 0xFF);
        shell_rx_push(&s_shell, ch);  // 内置缓冲区
    }
}

/* ==================== 初始化 ==================== */

/* shell_user_init() 定义在 shell_user.c 中 */
#if SHELL_USING_AUTH
extern void shell_user_init(shell_t* sh);
#endif

/**
 * @brief Shell初始化
 */
void my_shell_init(void)
{
    shell_config_t cfg = {
        .write = uart_write,
        .critical_enter = shell_port_critical_enter,
        .critical_exit = shell_port_critical_exit,
#if SHELL_USING_AUTH
        .password_verify = NULL,
#endif
    };
    shell_init(&s_shell, &cfg);

#if SHELL_USING_AUTH
    shell_user_init(&s_shell);
#endif

    /* 使能UART3接收中断 */
    USART3->CR1 |= USART_CR1_RXNEIE;
}

/**
 * @brief Shell任务（主循环调用）
 */
void my_shell_task(void)
{
    shell_task(&s_shell);
}
