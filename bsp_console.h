#ifndef BSP_CONSOLE_H_
#define BSP_CONSOLE_H_

#include "shell.h"

int bsp_console_init(void);
void bsp_console_task(void);
void bsp_console_usart3_irq_handler(void);

void bsp_console_start(void);
void bsp_console_write(const char* data, uint16_t len);
int bsp_console_read_irq_byte(uint8_t* ch);
uint32_t bsp_console_critical_enter(void);
void bsp_console_critical_exit(uint32_t state);

#endif
