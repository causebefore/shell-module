/**
 * @file    unity_config.h
 * @brief   Unity框架配置 (主机端GCC编译)
 */
#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

/* 使用标准 stdint.h 类型 */
#include <stdint.h>

/* 主机端使用 printf 输出测试结果 */
#include <stdio.h>
#define UNITY_OUTPUT_CHAR(c) putchar(c)
#define UNITY_OUTPUT_FLUSH() fflush(stdout)

/* 浮点断言支持 (shell 模块使用 float 变量) */
#define UNITY_INCLUDE_FLOAT

#endif /* UNITY_CONFIG_H */
