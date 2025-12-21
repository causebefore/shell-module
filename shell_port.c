/*****************************************************************************
 * @file        shell_port.c
 * @brief       Shell移植层实现 - 硬件IO函数
 * @author      liu
 * @date        2025-12-10
 * @version     2.0
 * @copyright   Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 *
 * @details     本文件仅实现Shell的硬件IO层，包括：
 *              - UART收发函数实现
 *              - 中断/轮询模式缓冲区管理
 *
 * @note        移植到其他平台时，只需修改本文件中的读写函数即可
 *              Shell实例初始化、用户配置等请在shell_demo.c中实现
 *****************************************************************************/

#include "shell_port.h"

/* ===========================================================================
 *                          IO函数实现区
 *                    (移植时主要修改这两个函数)
 * ===========================================================================*/

char c;

/**
 * @brief Shell写函数 - 发送数据到UART
 * @param data 要发送的数据指针
 * @param len 数据长度
 *
 * @note 移植说明：
 *       - STM32 HAL: HAL_UART_Transmit(&huart1, (uint8_t*)data, len, 100);
 *       - STM32 SPL: 循环调用 USART_SendData()
 *       - Linux: write(fd, data, len);
 *       - FreeRTOS: 可能需要加互斥锁
 */
static void shell_write_impl(const char *data, uint16_t len)
{
    /* 示例: STM32标准库实现 */
    //    for (uint16_t i = 0; i < len; i++)
    //    {
    //        Usart_SendByte(DEBUG_USARTx, data[i]);
    //    }
    SEGGER_RTT_SetTerminal(0);
    SEGGER_RTT_WriteString(0, data);
    /* 其他平台示例:
     * HAL库: HAL_UART_Transmit(&huart1, (uint8_t*)data, len, HAL_MAX_DELAY);
     * Linux: write(uart_fd, data, len);
     */
}

/**
 * @brief Shell读函数 - 从UART读取数据
 * @param data 接收缓冲区
 * @param len 要读取的长度
 * @return 实际读取的字节数
 *
 * @note 实现方式由用户自定义，可以是：
 *       - 轮询模式：直接检查UART寄存器
 *       - 中断模式：从环形缓冲区读取
 *       - DMA模式：从DMA缓冲区读取
 */
static int shell_read_impl(char *data, uint16_t len)
{
    (void)len; /* 未使用的参数 */

    //    /* 示例: 轮询模式 - STM32标准库实现 */
    //    if (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_RXNE) != RESET)
    //    {
    //        data[0] = USART_ReceiveData(DEBUG_USARTx);
    //        return 1;
    //    }

    /* 示例: 中断模式 - 从环形缓冲区读取
     * if (rx_write_index != rx_read_index)
     * {
     *     data[0] = rx_buffer[rx_read_index];
     *     rx_read_index = (rx_read_index + 1) % BUFFER_SIZE;
     *     return 1;
     * }
     */

    /* 示例: HAL库轮询模式
     * if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE))
     * {
     *     data[0] = huart1.Instance->DR;
     *     return 1;
     * }
     */

    return 0; /* 无数据 */
}
shell_t g_shell_usart1;
void my_shell_init(void)
{
    // 1. 初始化命令列表
    shell_commands_init();

    // 2. 初始化Shell实例
    shell_init_ex(&g_shell_usart1,
                  "RTTShell",
                  shell_commands,
                  shell_cmd_count,
                  shell_write_impl,
                  shell_read_impl);
}

void my_shell_task(void)
{
    shell_task(&g_shell_usart1);

    if (SEGGER_RTT_HasKey())
    {
        c = SEGGER_RTT_GetKey();
        shellHandler(&g_shell_usart1, c);
    }
}
