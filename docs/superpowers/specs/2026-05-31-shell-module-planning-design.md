# Shell Module 规划设计文档

## 1. 概述

### 1.1 目标
借鉴microshell的回调注入模式，重新设计shell-module的port层，实现5分钟移植，同时添加ISR日志队列、ANSI增强、密码验证回调等新功能。

### 1.2 核心改动
- 重新设计初始化API，使用配置结构体
- port层只实现write回调，read使用内置缓冲区
- 分离用户代码到独立文件
- 添加新功能

### 1.3 成功标准
- port层从220行简化到40行
- 移植时间从30分钟降低到5分钟
- 添加所有新功能
- 测试覆盖率90%以上

## 2. 架构设计

### 2.1 文件结构
```
shell/
├── shell.c          # 核心实现（命令解析、行编辑、历史、补全等）
├── shell.h          # 公共API
├── shell_cfg.h      # 配置宏
├── shell_port.c     # 硬件IO层（仅40行）
├── shell_user.c     # 用户命令/变量/用户表（新建）
├── shell_log.c      # ISR日志队列（新建）
└── shell_ansi.h     # ANSI颜色定义（新建）
```

### 2.2 职责划分
- **shell.c**：核心逻辑，不依赖硬件
- **shell.h**：公共API，配置结构体定义
- **shell_cfg.h**：编译时配置宏
- **shell_port.c**：硬件IO层，仅write回调和中断处理
- **shell_user.c**：用户命令、变量、用户表
- **shell_log.c**：ISR日志队列实现
- **shell_ansi.h**：ANSI颜色和光标控制定义

### 2.3 初始化流程
```
用户代码调用 shell_port_init()
    ↓
shell_port_init() 调用 shell_init_export()
    ↓
shell_init_export() 初始化shell_t结构体
    ↓
主循环调用 shell_task()
    ↓
shell_task() 读取缓冲区数据并处理
```

## 3. API设计

### 3.1 初始化API

```c
// 新的配置结构体
typedef struct {
    void (*write)(const char* data, uint16_t len);  // 必须实现
    shell_password_verify_fn_t password_verify;      // 可选
    // 可扩展：未来可添加其他配置项
} shell_config_t;

// 新的初始化函数
void shell_init(shell_t* sh, const shell_config_t* cfg);

// 便捷宏（使用导出命令）
#define shell_init_export(sh, write) shell_init(sh, &(shell_config_t){.write = write})
```

### 3.2 port层API

```c
// shell_port.c（仅40行）
#include "shell.h"

static shell_t s_shell;

// 硬件IO层（仅这个函数与硬件相关）
static void uart_write(const char* data, uint16_t len) {
    HAL_UART_Transmit(&huart3, (uint8_t*)data, len, 100);
}

// 中断处理（一行搞定）
void shell_uart3_irq_handler(void) {
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE)) {
        uint8_t ch = (uint8_t)(huart3.Instance->DR & 0xFF);
        shell_rx_push(&s_shell, ch);  // 内置缓冲区
    }
}

// 初始化（三行搞定）
void shell_port_init(void) {
    shell_init_export(&s_shell, uart_write);
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
}
```

### 3.3 用户代码API

```c
// shell_user.c
#include "shell.h"

// 用户命令
SHELL_EXPORT_CMD(test, "Test command", cmd_test, SHELL_PERM_USER);
SHELL_EXPORT_CMD(reboot, "System reboot", cmd_reboot, SHELL_PERM_ADMIN);

// 变量
SHELL_EXPORT_VAR(version, &s_version, SHELL_VAR_STRING);

// 用户表
#if SHELL_USING_AUTH
static const shell_user_t s_shell_users[] = {
    {"root",  (const char*)(uintptr_t)HASH_ROOT,  SHELL_PERM_ROOT},
    {"admin", (const char*)(uintptr_t)HASH_ADMIN, SHELL_PERM_ADMIN},
    {"guest", (const char*)(uintptr_t)HASH_NONE,  SHELL_PERM_USER},
};
#endif
```

## 4. 功能增强设计

### 4.1 ISR日志队列

**设计思路**：生产者-消费者模式，ISR快速写入，主循环慢速输出

```c
// shell_log.h
typedef struct {
    uint8_t buf[SHELL_LOG_QUEUE_SIZE];
    volatile uint16_t head;  // ISR写
    volatile uint16_t tail;  // 主循环读
    volatile uint16_t dropped_total;
    uint16_t dropped_reported;
} shell_log_queue_t;

// ISR中调用（快速，不阻塞）
int shell_log_isr(shell_t* sh, const uint8_t* data, uint16_t len);
int shell_log_text_isr(shell_t* sh, const char* s);

// 主循环调用（在shell_task中自动调用）
static void shell_log_drain(shell_t* sh);
```

**使用示例**：
```c
// 在UART中断中记录日志
void USART1_IRQHandler(void) {
    uint8_t ch = USART1->DR;
    shell_log_text_isr(&shell, "UART recv: ");
    shell_log_isr(&shell, &ch, 1);
    shell_log_text_isr(&shell, "\r\n");
}
```

### 4.2 ANSI增强

**设计思路**：支持VT100颜色和光标控制

```c
// shell_ansi.h
#define ANSI_CLEAR      "\033[2J\033[H"
#define ANSI_CLEARLN    "\033[2K\r"
#define ANSI_COLOR_RED  "\033[31m"
#define ANSI_COLOR_GREEN "\033[32m"
#define ANSI_COLOR_RESET "\033[0m"

// 带颜色的打印函数
void shell_print_color(shell_t* sh, const char* color, const char* str);
void shell_printf_color(shell_t* sh, const char* color, const char* fmt, ...);
```

**使用示例**：
```c
// 打印红色错误信息
shell_print_color(&shell, ANSI_COLOR_RED, "Error: file not found\r\n");

// 打印绿色成功信息
shell_print_color(&shell, ANSI_COLOR_GREEN, "OK\r\n");
```

### 4.3 密码验证回调

**设计思路**：可插拔的密码验证函数，支持自定义验证逻辑

```c
// shell.h
typedef int (*shell_password_verify_fn_t)(const shell_user_t* user, const char* input_password);

// 配置结构体中添加
typedef struct {
    void (*write)(const char* data, uint16_t len);
    shell_password_verify_fn_t password_verify;  // 可选
    // 可扩展：未来可添加其他配置项
} shell_config_t;
```

**使用示例**：
```c
// 自定义密码验证（支持哈希、外部存储等）
int my_password_verify(const shell_user_t* user, const char* input_password) {
    // 计算输入密码的哈希
    uint32_t hash = compute_hash(input_password);
    // 与存储的哈希比较
    return (hash == (uint32_t)(uintptr_t)user->password) ? 0 : -1;
}

// 初始化时配置
shell_config_t cfg = {
    .write = uart_write,
    .password_verify = my_password_verify,
};
shell_init(&shell, &cfg);
```

## 5. STM32 Demo

### 5.1 HAL库Demo

```c
// shell_port_hal.c
#include "shell.h"
#include "stm32f4xx_hal.h"

static shell_t s_shell;

static void uart_write(const char* data, uint16_t len) {
    HAL_UART_Transmit(&huart3, (uint8_t*)data, len, 100);
}

void shell_uart3_irq_handler(void) {
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE)) {
        uint8_t ch = (uint8_t)(huart3.Instance->DR & 0xFF);
        shell_rx_push(&s_shell, ch);
    }
}

void shell_port_init(void) {
    shell_init_export(&s_shell, uart_write);
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
}
```

### 5.2 标准外设库Demo

```c
// shell_port_stdperiph.c
#include "shell.h"
#include "stm32f4xx_usart.h"

static shell_t s_shell;

static void uart_write(const char* data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
        USART_SendData(USART3, data[i]);
    }
}

void shell_uart3_irq_handler(void) {
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) {
        uint8_t ch = (uint8_t)USART_ReceiveData(USART3);
        shell_rx_push(&s_shell, ch);
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
}

void shell_port_init(void) {
    shell_init_export(&s_shell, uart_write);
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
}
```

## 6. 测试设计

### 6.1 测试结构
```
test/
├── test_shell.c          # 核心功能测试（增强）
├── test_shell_log.c      # ISR日志队列测试（新建）
├── test_shell_ansi.c     # ANSI增强测试（新建）
├── test_shell_auth.c     # 密码验证回调测试（新建）
├── test_shell_mock.c     # Mock模块（增强）
├── test_shell_mock.h
└── unity/                # Unity框架（保持不变）
```

### 6.2 测试策略
1. **核心功能测试**：保持现有81个测试用例，添加port层简化后的测试
2. **ISR日志队列测试**：测试ISR写入和主循环读取，缓冲区满时的丢弃行为
3. **ANSI增强测试**：测试颜色输出、光标控制、清屏功能
4. **密码验证回调测试**：测试自定义验证函数、默认验证逻辑、错误密码处理

### 6.3 测试覆盖目标
- 核心功能：100%覆盖
- 新功能：90%覆盖
- 边界条件：100%覆盖

## 7. 实施计划

### 7.1 阶段1：架构重构（1-2周）
1. 重新设计初始化API，使用配置结构体
2. 重构port层，移除LWRB依赖
3. 分离用户代码到`shell_user.c`
4. 更新测试框架

### 7.2 阶段2：功能增强（2-3周）
1. 实现ISR日志队列
2. 实现ANSI增强
3. 实现密码验证回调
4. 编写新功能的测试

### 7.3 阶段3：优化完善（1周）
1. 性能优化
2. 文档更新
3. 示例代码
4. 最终测试

### 7.4 里程碑
- **M1**：架构重构完成，port层简化到40行
- **M2**：功能增强完成，所有新功能可用
- **M3**：优化完善完成，质量达标

## 8. 风险控制

### 8.1 API兼容性
- 提供旧API的兼容宏
- 逐步迁移，不强制一次性切换

### 8.2 测试覆盖
- 每个阶段都有完整测试
- 关键改动需要代码审查

### 8.3 回滚机制
- 每个阶段都可以回滚
- 保持git提交历史清晰

## 9. 总结

本设计方案借鉴microshell的回调注入模式，重新设计shell-module的port层，实现5分钟移植。同时添加ISR日志队列、ANSI增强、密码验证回调等新功能，提升shell-module的功能性和易用性。

通过分阶段实施和风险控制，确保项目质量和进度。预计6-8周完成所有改进，达到生产可用水平。
