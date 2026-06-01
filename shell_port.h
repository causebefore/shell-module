#ifndef SHELL_PORT_H_
#define SHELL_PORT_H_

#include "shell.h"

typedef void (*shell_port_write_fn_t)(void* ctx, const char* data, uint16_t len);
typedef uint32_t (*shell_port_critical_enter_fn_t)(void* ctx);
typedef void (*shell_port_critical_exit_fn_t)(void* ctx, uint32_t state);
typedef int (*shell_port_start_fn_t)(void* ctx);

typedef struct
{
    void*                          ctx;
    shell_port_write_fn_t          write;
    shell_port_critical_enter_fn_t critical_enter;
    shell_port_critical_exit_fn_t  critical_exit;
    shell_port_start_fn_t          start;
} shell_port_backend_t;

int shell_port_init(const shell_port_backend_t* backend);
void shell_port_task(void);
shell_t* shell_port_get_shell(void);

/* ISR/DMA callback entry points. These only feed the shell RX buffer. */
void shell_port_rx_from_isr(uint8_t ch);
void shell_port_rx_buf_from_isr(const uint8_t* data, uint16_t len);

#endif
