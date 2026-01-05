/*****************************************************************************
 * @file        shell_ext.c
 * @brief       Shell扩展功能 - 函数签名适配实现
 * @author      liu
 * @date        2025-12-09
 * @version     2.0
 * @copyright   Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 *
 * @details     本文件实现函数签名适配功能，允许Shell命令直接调用C函数
 *              自动进行参数类型转换，支持最多SHELL_PARAMETER_MAX_NUMBER个参数
 *****************************************************************************/

#include "shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if SHELL_USING_FUNC_SIGNATURE

/**
 * @brief 执行函数签名适配命令
 * @param shell Shell实例指针
 * @param command 命令结构指针
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 命令执行返回值
 *
 * @note 此函数根据paramNum自动适配不同参数数量的函数调用
 *       - paramNum=0: 使用标准main签名 int func(int argc, char *argv[])
 *       - paramNum>0: 使用size_t参数适配 int func(size_t p1, size_t p2, ...)
 *       支持1-8个参数的函数（可通过SHELL_PARAMETER_MAX_NUMBER扩展）
 */
int shellExtRun(shell_t* shell, shell_cmd_t* command, int argc, char* argv[])
{
    if (command == NULL || shell == NULL)
    {
        return -1;
    }

    int ret = 0;

    /* paramNum=0 表示使用标准main形式: int func(int argc, char *argv[]) */
    if (command->paramNum == 0)
    {
        int (*func)(int, char**) = (int (*)(int, char**)) command->function;
        return func(argc, argv);
    }

    /* paramNum>0 表示使用参数适配模式 */
    size_t params[SHELL_PARAMETER_MAX_NUMBER] = {0};
    int    paramNum                           = command->paramNum > (argc - 1) ? command->paramNum : (argc - 1);
    for (int i = 0; i < paramNum; i++)
    {
        params[i] = (size_t) argv[i + 1];
    }
    switch (paramNum)
    {
    #if SHELL_PARAMETER_MAX_NUMBER >= 1
        case 1: {
            int (*func)(size_t) = command->function;
            ret                 = func(params[0]);
            break;
        }
    #endif
    #if SHELL_PARAMETER_MAX_NUMBER >= 2
        case 2: {
            int (*func2)(size_t, size_t) = command->function;
            ret                          = func2(params[0], params[1]);
            break;
        }
    #endif
    #if SHELL_PARAMETER_MAX_NUMBER >= 3
        case 3: {
            int (*func3)(size_t, size_t, size_t) = command->function;
            ret                                  = func3(params[0], params[1], params[2]);
            break;
        }
    #endif
    #if SHELL_PARAMETER_MAX_NUMBER >= 4
        case 4: {
            int (*func4)(size_t, size_t, size_t, size_t) = command->function;
            ret                                          = func4(params[0], params[1], params[2], params[3]);
            break;
        }
    #endif
    #if SHELL_PARAMETER_MAX_NUMBER >= 5
        case 5: {
            int (*func5)(size_t, size_t, size_t, size_t, size_t) = command->function;
            ret = func5(params[0], params[1], params[2], params[3], params[4]);
            break;
        }
    #endif
    #if SHELL_PARAMETER_MAX_NUMBER >= 6
        case 6: {
            int (*func6)(size_t, size_t, size_t, size_t, size_t, size_t) = command->function;
            ret = func6(params[0], params[1], params[2], params[3], params[4], params[5]);
            break;
        }
    #endif
    #if SHELL_PARAMETER_MAX_NUMBER >= 7
        case 7: {
            int (*func7)(size_t, size_t, size_t, size_t, size_t, size_t, size_t) = command->function;
            ret = func7(params[0], params[1], params[2], params[3], params[4], params[5], params[6]);
            break;
        }
    #endif
    #if SHELL_PARAMETER_MAX_NUMBER >= 8
        case 8: {
            int (*func8)(size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t) = command->function;
            ret = func8(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7]);
            break;
        }
    #endif
        default:
            ret = -1;
            break;
    }
    return ret;
}

#else

/**
 * @brief 函数签名适配功能未启用时的占位函数
 * @note 当SHELL_USING_FUNC_SIGNATURE=0时，此函数用于避免链接错误
 */
int shellExtRun(shell_t* shell, shell_cmd_t* command, int argc, char* argv[])
{
    (void) shell;
    (void) command;
    (void) argc;
    (void) argv;
    return -1; /* 功能未启用 */
}

#endif /* SHELL_USING_FUNC_SIGNATURE */

/* ===========================================================================
 *                          使用说明
 * ===========================================================================
 *
 * 函数签名适配功能允许Shell命令直接调用C函数，无需编写main形式的包装函数
 *
 * 示例1: 控制LED（1个参数）
 * --------------------------------
 * // C函数定义
 * int led_control(int state)
 * {
 *     if (state) LED_ON();
 *     else LED_OFF();
 *     return 0;
 * }
 *
 * // 命令注册
 * {"led", "Control LED", led_control, SHELL_AUTH_USER, SHELL_TYPE_CMD_FUNC, 1}
 *
 * // Shell中使用
 * shell> led 1     // 打开LED
 * shell> led 0     // 关闭LED
 *
 * 示例2: 设置PWM占空比（2个参数）
 * --------------------------------
 * // C函数定义
 * int pwm_set(int channel, int duty)
 * {
 *     PWM_SetDuty(channel, duty);
 *     return 0;
 * }
 *
 * // 命令注册
 * {"pwm", "Set PWM duty", pwm_set, SHELL_AUTH_USER, SHELL_TYPE_CMD_FUNC, 2}
 *
 * // Shell中使用
 * shell> pwm 1 50  // 设置通道1占空比为50%
 *
 * 注意事项:
 * 1. 函数参数类型会被转换为size_t（通常是unsigned int或unsigned long）
 * 2. 不支持浮点数参数（需要自行转换，如传入100倍的整数）
 * 3. 字符串参数会被转换为指针（需谨慎使用）
 * 4. 最大支持参数数量由SHELL_PARAMETER_MAX_NUMBER决定
 * 5. 命令注册时type必须设置为SHELL_TYPE_CMD_FUNC
 * 6. paramNum必须正确设置为函数的参数数量
 *
 * ===========================================================================
 */
