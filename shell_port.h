#ifndef __SHELL_PORT_H
#define __SHELL_PORT_H

#include "shell.h"

void my_shell_init(void);
void my_shell_task(void);

extern shell_t g_shell_usart1;

#endif
