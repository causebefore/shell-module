/**
 * @file    shell_port_rtt.h
 * @brief   SEGGER RTT 后端 — shell 使用 RTT 输入输出
 *
 * 使用 RTT Channel 0 作为 shell 的传输通道。
 * 调试用: 通过 J-Link RTT Viewer / Client 交互。
 */

#ifndef SHELL_PORT_RTT_H_
#define SHELL_PORT_RTT_H_

#include "shell_port.h"

/**
 * @brief  初始化 shell 并绑定 RTT 后端
 * @return 0 成功, -1 失败
 */
int shell_port_rtt_init(void);

/**
 * @brief  shell 主循环任务 (在主循环或 FreeRTOS 任务中调用)
 */
void shell_port_rtt_task(void);

/**
 * @brief  获取 shell 实例指针
 */
shell_t* shell_port_rtt_get_shell(void);

#endif
