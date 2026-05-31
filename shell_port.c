/**
 * @file    shell_port.c
 * @brief   Shell移植层 - 简化版
 *
 * 仅包含硬件IO和中断处理，约40行代码
 */

#include "shell.h"

/* Shell实例 */
static shell_t s_shell;

/* ==================== 硬件IO层 ==================== */

/**
 * @brief Shell写函数 - 阻塞发送
 */
static void uart_write(const char* data, uint16_t len)
{
    /* 阻塞发送，简单可靠 */
    for (uint16_t i = 0; i < len; i++)
    {
        /* 等待发送寄存器空 */
        while (!(USART3->SR & USART_SR_TXE))
            ;
        USART3->DR = data[i];
    }
    /* 等待发送完成 */
    while (!(USART3->SR & USART_SR_TC))
        ;
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
    shell_init_export(&s_shell, uart_write);

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
