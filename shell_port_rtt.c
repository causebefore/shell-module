/**
 * @file    shell_port_rtt.c
 * @brief   SEGGER RTT 后端实现 — shell 使用 RTT Channel 0 输入输出
 *
 * 数据流:
 *   输入: RTT Viewer 键盘 → SEGGER_RTT Read → shell read 回调 → shell_input
 *   输出: shell write → SEGGER_RTT_Write → RTT Viewer 显示
 *   mlog: mlog → shell_log_isr → shell drain → shell write → SEGGER_RTT_Write
 */

#include "shell_port_rtt.h"
#include "SEGGER_RTT.h"

#include <stdint.h>
#if defined(__ARM_ARCH) || defined(__ARMCC_VERSION) || defined(__ICCARM__)
    #include "cmsis_compiler.h" /* __get_PRIMASK, __disable_irq, __set_PRIMASK */
#endif

#if SHELL_USING_AUTH
extern void shell_user_init(shell_t* sh);
#endif

/* ==================== RTT 后端回调 ==================== */

static void rtt_write(void* ctx, const char* data, uint16_t len)
{
    (void) ctx;
    SEGGER_RTT_Write(0, data, (unsigned) len);
}

static int rtt_read(void* ctx, char* buf, uint16_t max_len)
{
    (void) ctx;
    /* SEGGER_RTT_Read 返回实际读取的字节数 */
    return (int) SEGGER_RTT_Read(0, buf, (unsigned) max_len);
}

static uint32_t rtt_critical_enter(void* ctx)
{
    (void) ctx;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void rtt_critical_exit(void* ctx, uint32_t state)
{
    (void) ctx;
    __set_PRIMASK(state);
}

/* ==================== 后端实例 ==================== */

static const shell_port_backend_t s_rtt_backend = {
    .ctx = NULL,
    .write = rtt_write,
    .read = rtt_read,
    .critical_enter = rtt_critical_enter,
    .critical_exit = rtt_critical_exit,
    .start = NULL, /* RTT 无需硬件启动 */
};

/* ==================== 公共 API ==================== */

int shell_port_rtt_init(void)
{
    int ret = shell_port_init(&s_rtt_backend);
    if (ret != 0)
    {
        return ret;
    }

#if SHELL_USING_AUTH
    shell_user_init(shell_port_get_shell());
#endif

    return 0;
}

void shell_port_rtt_task(void)
{
    shell_port_task();
}

shell_t* shell_port_rtt_get_shell(void)
{
    return shell_port_get_shell();
}
