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

/* shell_user_init() 定义在 shell_user.c 中 */
#if SHELL_USING_AUTH
extern void shell_user_init(shell_t* sh);
#endif

/* ==================== 初始化 ==================== */

void my_shell_init(void)
{
    /* 初始化LWRB环形缓冲区 */
    lwrb_init(&s_tx_rb, s_tx_buf, sizeof(s_tx_buf));
    lwrb_init(&s_rx_rb, s_rx_buf, sizeof(s_rx_buf));

    /* 初始化shell (使用宏导出命令, 命令表自动从链接段加载) */
    shell_init_export(&g_shell_usart3, shell_write_impl);
    g_shell_usart3.read = shell_read_impl;

#if SHELL_USING_AUTH
    /* 设置用户列表 (用户表定义在 shell_user.c) */
    shell_user_init(&g_shell_usart3);
#endif

    /* 启用UART3接收中断 */
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
}

void my_shell_task(void)
{
    shell_task(&g_shell_usart3);
}
