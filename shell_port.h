#ifndef __SHELL_PORT_H
#define __SHELL_PORT_H

#include "shell.h"
#include <stdio.h>
#include <string.h>
#include "bsp_usart.h" /* 根据实际硬件修改头文件 */

void my_shell_init(void);
void my_shell_task(void);

extern shell_t g_shell_usart1;

#endif /* __SHELL_PORT_H */
