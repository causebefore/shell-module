#ifndef __SHELL_PORT_H
#define __SHELL_PORT_H

#include "shell.h"

void my_shell_init(void);
void my_shell_task(void);

/* UART3中断处理函数 - 需要在 USART3_IRQHandler 中调用 */
void shell_uart3_irq_handler(void);

extern shell_t g_shell_usart3;

#endif
