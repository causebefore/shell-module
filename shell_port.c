/**
 * @file    shell_port.c
 * @brief   Shell transport adapter core
 */

#include "shell_port.h"

#include <stddef.h>
#include <string.h>

static shell_t s_shell;
static const shell_port_backend_t* s_backend;

/* ==================== trampoline 回调 ==================== */

static void shell_port_write_trampoline(const char* data, uint16_t len)
{
    if (s_backend && s_backend->write)
    {
        s_backend->write(s_backend->ctx, data, len);
    }
}

static int shell_port_read_trampoline(char* buf, uint16_t max_len)
{
    if (s_backend && s_backend->read)
    {
        return s_backend->read(s_backend->ctx, buf, max_len);
    }
    return 0;
}

static uint32_t shell_port_critical_enter_trampoline(void)
{
    if (s_backend && s_backend->critical_enter && s_backend->critical_exit)
    {
        return s_backend->critical_enter(s_backend->ctx);
    }
    return 0u;
}

static void shell_port_critical_exit_trampoline(uint32_t state)
{
    if (s_backend && s_backend->critical_enter && s_backend->critical_exit)
    {
        s_backend->critical_exit(s_backend->ctx, state);
    }
}

/* ==================== 公共 API ==================== */

static void shell_port_reset(void)
{
    s_backend = NULL;
    memset(&s_shell, 0, sizeof(s_shell));
    g_shell = NULL;
}

int shell_port_init(const shell_port_backend_t* backend)
{
    if (!backend || !backend->write)
    {
        return -1;
    }

    s_backend = backend;

    shell_config_t cfg = {
        .write = shell_port_write_trampoline,
        .critical_enter = shell_port_critical_enter_trampoline,
        .critical_exit = shell_port_critical_exit_trampoline,
#if SHELL_USING_AUTH
        .password_verify = NULL,
#endif
    };
    shell_init(&s_shell, &cfg);

    /* 如果 backend 提供了 read 回调, 直接使用 (如 RTT 轮询) */
    if (backend->read)
    {
        s_shell.read = shell_port_read_trampoline;
    }

    if (backend->start)
    {
        int ret = backend->start(backend->ctx);
        if (ret != 0)
        {
            shell_port_reset();
            return ret;
        }
    }

    return 0;
}

void shell_port_task(void)
{
    shell_task(&s_shell);
}

shell_t* shell_port_get_shell(void)
{
    return &s_shell;
}

void shell_port_rx_from_isr(uint8_t ch)
{
#if SHELL_RX_BUF_SIZE > 0
    shell_rx_push(&s_shell, ch);
#else
    (void) ch;
#endif
}

void shell_port_rx_buf_from_isr(const uint8_t* data, uint16_t len)
{
#if SHELL_RX_BUF_SIZE > 0
    shell_rx_push_buf(&s_shell, data, len);
#else
    (void) data;
    (void) len;
#endif
}
