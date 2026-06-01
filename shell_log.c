/**
 *
 * @file shell_log.c
 * @author liu (lbq08@foxmail.com)
 * @brief ISR日志队列 - 中断安全的日志缓冲
 *
 * @copyright Copyright (c) 2026 liu
 * For study and research only
 */
#include "shell.h"

#include <stdio.h>
#include <string.h>

#if SHELL_USING_LOG_QUEUE

/* 编译时校验: SHELL_LOG_QUEUE_SIZE 必须是2的幂 */
#if (SHELL_LOG_QUEUE_SIZE & (SHELL_LOG_QUEUE_SIZE - 1)) != 0
    #error "SHELL_LOG_QUEUE_SIZE must be a power of 2"
#endif

static uint32_t shell_log_lock(shell_t* sh)
{
    if (sh && sh->critical_enter && sh->critical_exit)
    {
        return sh->critical_enter();
    }
    return 0u;
}

static void shell_log_unlock(shell_t* sh, uint32_t state)
{
    if (sh && sh->critical_enter && sh->critical_exit)
    {
        sh->critical_exit(state);
    }
}

static uint16_t shell_strnlen_isr(const char* s, uint16_t max_len)
{
    uint16_t len = 0;
    while (len < max_len && s[len] != '\0')
    {
        len++;
    }
    return len;
}

/**
 * @brief 向日志队列写入原始数据 (ISR安全)
 * @param sh   shell实例
 * @param data 数据指针
 * @param len  数据长度
 * @return 实际写入的字节数
 */
int shell_log_isr(shell_t* sh, const uint8_t* data, uint16_t len)
{
    if (!sh || !data || len == 0)
    {
        return 0;
    }

    uint16_t written = 0;
    uint32_t lock_state = shell_log_lock(sh);
    uint16_t head    = sh->log_head;

    for (uint16_t i = 0; i < len; i++)
    {
        uint16_t next = (head + 1) & (SHELL_LOG_QUEUE_SIZE - 1);
        if (next == sh->log_tail)
        {
            /* 队列满，记录丢弃 */
            sh->log_dropped_total += (len - written);
            break;
        }
        sh->log_buf[head] = data[i];
        head = next;
        written++;
    }

    __asm volatile("" ::: "memory"); /* 确保数据写入先于 head 更新 */
    sh->log_head = head;
    shell_log_unlock(sh, lock_state);
    return (int) written;
}

/**
 * @brief 向日志队列写入文本 (ISR安全)
 * @param sh shell实例
 * @param s  以null结尾的字符串
 * @return 实际写入的字节数 (不含null终止符)
 */
int shell_log_text_isr(shell_t* sh, const char* s)
{
    if (!sh || !s)
    {
        return 0;
    }

    uint16_t len = shell_strnlen_isr(s, SHELL_LOG_QUEUE_SIZE - 1);
    return shell_log_isr(sh, (const uint8_t*) s, len);
}

/**
 * @brief 从日志队列取出数据并输出 (主循环调用)
 * @param sh shell实例
 */
void shell_log_drain(shell_t* sh)
{
    if (!sh || !sh->write)
    {
        return;
    }

    uint32_t lock_state = shell_log_lock(sh);
    uint16_t tail = sh->log_tail;
    uint16_t head = sh->log_head;
    __asm volatile("" ::: "memory"); /* 确保先读取 head 再访问数据 */
    shell_log_unlock(sh, lock_state);

    if (tail == head)
    {
        /* 队列为空 */
        return;
    }

    /*
     * 输出丢弃报告 (如果有)
     * 只在有新丢弃时报告，避免重复输出
     */
    if (sh->log_dropped_total != sh->log_dropped_reported)
    {
        lock_state = shell_log_lock(sh);
        uint16_t dropped = sh->log_dropped_total - sh->log_dropped_reported;
        sh->log_dropped_reported = sh->log_dropped_total;
        shell_log_unlock(sh, lock_state);

        char drop_msg[48];
        int  n = snprintf(drop_msg, sizeof(drop_msg),
                          "\r\n[log dropped %u bytes]\r\n", dropped);
        if (n > 0)
        {
            if (n > (int) sizeof(drop_msg) - 1)
            {
                n = sizeof(drop_msg) - 1;
            }
            sh->write(drop_msg, (uint16_t) n);
        }
    }

    /*
     * 分两段输出 (不跨越环绕点):
     *   段1: tail -> min(head, QUEUE_SIZE)
     *   段2: 如果 head < tail, 0 -> head
     */
    uint16_t chunk1 = (head >= tail) ? (head - tail) : (SHELL_LOG_QUEUE_SIZE - tail);
    if (chunk1 > 0)
    {
        sh->write((const char*) &sh->log_buf[tail], chunk1);
    }

    if (head < tail)
    {
        /* 环绕: 输出第二段 0 -> head */
        if (head > 0)
        {
            sh->write((const char*) &sh->log_buf[0], head);
        }
    }

    __asm volatile("" ::: "memory"); /* 确保数据读取完成后再更新 tail */
    lock_state = shell_log_lock(sh);
    sh->log_tail = head;
    shell_log_unlock(sh, lock_state);
}

#endif /* SHELL_USING_LOG_QUEUE */
