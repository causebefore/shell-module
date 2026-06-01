/**
 * @file    bsp_console.c
 * @brief   Board console backend for the shell module
 */

#include "bsp_console.h"
#include "shell_port.h"

#include "main.h"

#ifndef BSP_CONSOLE_TX_TIMEOUT
#define BSP_CONSOLE_TX_TIMEOUT 1000000u
#endif

#if SHELL_USING_AUTH
extern void shell_user_init(shell_t* sh);
#endif

static int bsp_console_wait_flag(uint32_t flag)
{
    uint32_t timeout = BSP_CONSOLE_TX_TIMEOUT;
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

void bsp_console_start(void)
{
    USART3->CR1 |= USART_CR1_RXNEIE;
}

void bsp_console_write(const char* data, uint16_t len)
{
    if (!data)
    {
        return;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        if (!bsp_console_wait_flag(USART_SR_TXE))
        {
            return;
        }
        USART3->DR = (uint8_t) data[i];
    }
    (void) bsp_console_wait_flag(USART_SR_TC);
}

int bsp_console_read_irq_byte(uint8_t* ch)
{
    if (!ch || ((USART3->SR & USART_SR_RXNE) == 0u))
    {
        return 0;
    }

    *ch = (uint8_t) (USART3->DR & 0xFFu);
    return 1;
}

uint32_t bsp_console_critical_enter(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

void bsp_console_critical_exit(uint32_t state)
{
    __set_PRIMASK(state);
}

static void bsp_console_backend_write(void* ctx, const char* data, uint16_t len)
{
    (void) ctx;
    bsp_console_write(data, len);
}

static uint32_t bsp_console_backend_critical_enter(void* ctx)
{
    (void) ctx;
    return bsp_console_critical_enter();
}

static void bsp_console_backend_critical_exit(void* ctx, uint32_t state)
{
    (void) ctx;
    bsp_console_critical_exit(state);
}

static int bsp_console_backend_start(void* ctx)
{
    (void) ctx;
    bsp_console_start();
    return 0;
}

static const shell_port_backend_t s_bsp_console_backend = {
    .ctx = NULL,
    .write = bsp_console_backend_write,
    .critical_enter = bsp_console_backend_critical_enter,
    .critical_exit = bsp_console_backend_critical_exit,
    .start = bsp_console_backend_start,
};

int bsp_console_init(void)
{
    int ret = shell_port_init(&s_bsp_console_backend);
    if (ret != 0)
    {
        return ret;
    }

#if SHELL_USING_AUTH
    shell_user_init(shell_port_get_shell());
#endif

    return 0;
}

void bsp_console_task(void)
{
    shell_port_task();
}

void bsp_console_usart3_irq_handler(void)
{
    uint8_t ch;
    if (bsp_console_read_irq_byte(&ch))
    {
        shell_port_rx_from_isr(ch);
    }
}
