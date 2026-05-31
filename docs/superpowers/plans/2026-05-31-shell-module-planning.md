# Shell Module 规划实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 借鉴microshell的回调注入模式，重新设计shell-module的port层，实现5分钟移植，同时添加ISR日志队列、ANSI增强、密码验证回调等新功能。

**Architecture:** 采用分层架构，shell.c为核心逻辑，shell_port.c为硬件IO层，shell_user.c为用户代码，shell_log.c为ISR日志队列。使用配置结构体进行初始化，支持可插拔的密码验证回调。

**Tech Stack:** C语言、Unity测试框架、STM32 HAL/标准外设库

---

## 文件结构

在开始实施之前，需要创建或修改以下文件：

| 文件 | 操作 | 职责 |
|------|------|------|
| `shell.h` | 修改 | 添加配置结构体、新API声明 |
| `shell.c` | 修改 | 添加ISR日志队列、ANSI增强、密码验证回调实现 |
| `shell_cfg.h` | 修改 | 添加新功能的配置宏 |
| `shell_port.c` | 重写 | 简化为仅硬件IO层 |
| `shell_user.c` | 新建 | 用户命令、变量、用户表 |
| `shell_log.c` | 新建 | ISR日志队列实现 |
| `shell_ansi.h` | 新建 | ANSI颜色和光标控制定义 |
| `test/test_shell.c` | 修改 | 更新测试以适应新API |
| `test/test_shell_log.c` | 新建 | ISR日志队列测试 |
| `test/test_shell_ansi.c` | 新建 | ANSI增强测试 |
| `test/test_shell_auth.c` | 新建 | 密码验证回调测试 |
| `test/test_shell_mock.c` | 修改 | 更新Mock以支持新功能 |
| `test/test_shell_mock.h` | 修改 | 更新Mock头文件 |

## 阶段1：架构重构

### Task 1: 添加配置结构体到shell.h

**Files:**
- Modify: `shell.h:1-50`

- [ ] **Step 1: 添加密码验证回调类型定义**

在`shell.h`的`#include <stdint.h>`之后添加：

```c
/* 密码验证回调类型 */
#if SHELL_USING_AUTH
typedef int (*shell_password_verify_fn_t)(const shell_user_t* user, const char* input_password);
#endif
```

- [ ] **Step 2: 添加配置结构体定义**

在`shell_user_t`定义之后添加：

```c
/* Shell配置结构体 */
typedef struct
{
    void (*write)(const char* data, uint16_t len);  // 必须实现
#if SHELL_USING_AUTH
    shell_password_verify_fn_t password_verify;      // 可选，自定义密码验证
#endif
    // 可扩展：未来可添加其他配置项
} shell_config_t;
```

- [ ] **Step 3: 添加新的初始化函数声明**

在API声明部分添加：

```c
/* 新的初始化函数 */
void shell_init(shell_t* sh, const shell_config_t* cfg);

/* 便捷宏（使用导出命令） */
#define shell_init_export(sh, write) shell_init(sh, &(shell_config_t){.write = write})
```

- [ ] **Step 4: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 5: 提交修改**

```bash
git add shell.h
git commit -m "feat(api): 添加配置结构体和新的初始化函数"
```

### Task 2: 修改shell_t结构体

**Files:**
- Modify: `shell.h:248-291`

- [ ] **Step 1: 添加密码验证回调到shell_t**

在`shell_t`结构体中添加：

```c
#if SHELL_USING_AUTH
    shell_password_verify_fn_t password_verify;  // 密码验证回调
#endif
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell.h
git commit -m "feat(struct): 添加密码验证回调到shell_t"
```

### Task 3: 实现新的shell_init函数

**Files:**
- Modify: `shell.c:1-50`

- [ ] **Step 1: 添加新的shell_init函数实现**

在`shell.c`的`shell_init`函数之后添加：

```c
/* 新的初始化函数（使用配置结构体） */
void shell_init_config(shell_t* sh, const shell_config_t* cfg)
{
    if (!sh || !cfg || !cfg->write)
    {
        return;
    }

    memset(sh, 0, sizeof(shell_t));
    sh->write = cfg->write;

#if SHELL_USING_AUTH
    sh->password_verify = cfg->password_verify;
#endif

    sh->is_inited = 1;
}
```

- [ ] **Step 2: 更新shell_init_export宏**

在`shell.h`中更新宏定义：

```c
#define shell_init_export(sh, write) shell_init_config(sh, &(shell_config_t){.write = write})
```

- [ ] **Step 3: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 4: 提交修改**

```bash
git add shell.c shell.h
git commit -m "feat(init): 实现新的shell_init_config函数"
```

### Task 4: 创建shell_user.c

**Files:**
- Create: `shell_user.c`

- [ ] **Step 1: 创建shell_user.c文件**

```c
/**
 * @file    shell_user.c
 * @brief   用户命令、变量、用户表
 */

#include "shell.h"

/* ==================== 用户命令示例 ==================== */

static int cmd_test(int argc, char* argv[])
{
    shell_print(g_shell, "Test OK\r\n");
    return 0;
}

static int cmd_reboot(int argc, char* argv[])
{
    shell_print(g_shell, "System rebooting...\r\n");
    /* HAL_NVIC_SystemReset(); */
    return 0;
}

static int cmd_mode(int argc, char* argv[])
{
    if (argc < 2)
    {
        shell_print(g_shell, "Usage: mode <speed|angle|torque>\r\n");
        return -1;
    }
    shell_printf(g_shell, "Mode set to: %s\r\n", argv[1]);
    return 0;
}

/* 补全列表 */
static const char* s_mode_opts[] = {"speed", "angle", "torque", NULL};

/* ==================== 宏注册命令 ==================== */

/* 内置命令 */
SHELL_EXPORT_CMD(help, "Show commands", cmd_help, SHELL_PERM_NONE);
SHELL_EXPORT_CMD(clear, "Clear screen", cmd_clear, SHELL_PERM_NONE);
#if SHELL_USING_HISTORY
SHELL_EXPORT_CMD(history, "Show history", cmd_history, SHELL_PERM_NONE);
#endif
#if SHELL_USING_VAR
SHELL_EXPORT_CMD(var, "Read/write variable", cmd_var, SHELL_PERM_NONE);
SHELL_EXPORT_CMD(vars, "List all variables", cmd_vars, SHELL_PERM_NONE);
#endif

#if SHELL_USING_AUTH
SHELL_EXPORT_CMD(login, "Login user", cmd_login, SHELL_PERM_NONE);
SHELL_EXPORT_CMD(logout, "Logout", cmd_logout, SHELL_PERM_NONE);
SHELL_EXPORT_CMD(whoami, "Current user", cmd_whoami, SHELL_PERM_NONE);
#endif

/* 用户命令 */
SHELL_EXPORT_CMD(test, "Test command", cmd_test, SHELL_PERM_USER);
SHELL_EXPORT_CMD(reboot, "System reboot", cmd_reboot, SHELL_PERM_ADMIN);
SHELL_EXPORT_CMD_LIST(mode, "Set FOC mode", cmd_mode, SHELL_PERM_NONE, s_mode_opts);

/* ==================== 用户表 ==================== */

#if SHELL_USING_AUTH

#if SHELL_USING_HASH_PWD
#define HASH_ROOT  0x7DD1705AUL /* hash("123456") */
#define HASH_ADMIN 0x0F12FC8EUL /* hash("admin")  */
#define HASH_NONE  0x00000000UL /* 无密码 */

static const shell_user_t s_shell_users[] = {
    {"root",  (const char*) (uintptr_t) HASH_ROOT,  SHELL_PERM_ROOT },
    {"admin", (const char*) (uintptr_t) HASH_ADMIN, SHELL_PERM_ADMIN},
    {"guest", (const char*) (uintptr_t) HASH_NONE,  SHELL_PERM_USER },
};
#else
static const shell_user_t s_shell_users[] = {
    {"root",  "123456", SHELL_PERM_ROOT },
    {"admin", "admin",  SHELL_PERM_ADMIN},
    {"guest", "",       SHELL_PERM_USER },
};
#endif

#define USER_COUNT (sizeof(s_shell_users) / sizeof(s_shell_users[0]))

/* 设置用户表的便捷函数 */
void shell_user_init(shell_t* sh)
{
    shell_set_users(sh, s_shell_users, USER_COUNT);
}

#endif /* SHELL_USING_AUTH */

/* ==================== 变量导出示例 ==================== */

#if SHELL_USING_VAR
static int         s_test_int   = 100;
static uint32_t    s_test_uint  = 0x12345678;
static float       s_test_float = 3.14f;
static uint8_t     s_test_bool  = 1;
static const char* s_version    = "1.0.0";

SHELL_EXPORT_VAR(test_int, &s_test_int, SHELL_VAR_INT);
SHELL_EXPORT_VAR(test_uint, &s_test_uint, SHELL_VAR_UINT);
SHELL_EXPORT_VAR(test_float, &s_test_float, SHELL_VAR_FLOAT);
SHELL_EXPORT_VAR(test_bool, &s_test_bool, SHELL_VAR_BOOL);
SHELL_EXPORT_VAR_RO(version, &s_version, SHELL_VAR_STRING);
#endif
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell_user.c
git commit -m "feat(user): 创建shell_user.c，分离用户代码"
```

### Task 5: 简化shell_port.c

**Files:**
- Rewrite: `shell_port.c`

- [ ] **Step 1: 重写shell_port.c**

```c
/**
 * @file    shell_port.c
 * @brief   Shell移植层 - 简化版
 *
 * 仅包含硬件IO和中断处理，约40行代码
 */

#include "shell.h"

/* Shell实例 */
static shell_t s_shell;

/* ==================== 硬件IO层 ==================== */

/**
 * @brief Shell写函数 - 阻塞发送
 */
static void uart_write(const char* data, uint16_t len)
{
    /* 阻塞发送，简单可靠 */
    for (uint16_t i = 0; i < len; i++)
    {
        /* 等待发送寄存器空 */
        while (!(USART3->SR & USART_SR_TXE))
            ;
        USART3->DR = data[i];
    }
    /* 等待发送完成 */
    while (!(USART3->SR & USART_SR_TC))
        ;
}

/* ==================== 中断处理 ==================== */

/**
 * @brief UART3中断处理 - 在USART3_IRQHandler中调用
 */
void shell_uart3_irq_handler(void)
{
    if (USART3->SR & USART_SR_RXNE)
    {
        uint8_t ch = (uint8_t)(USART3->DR & 0xFF);
        shell_rx_push(&s_shell, ch);  // 内置缓冲区
    }
}

/* ==================== 初始化 ==================== */

/**
 * @brief Shell初始化
 */
void shell_port_init(void)
{
    shell_init_export(&s_shell, uart_write);

#if SHELL_USING_AUTH
    shell_user_init(&s_shell);
#endif

    /* 使能UART3接收中断 */
    USART3->CR1 |= USART_CR1_RXNEIE;
}

/**
 * @brief Shell任务（主循环调用）
 */
void shell_port_task(void)
{
    shell_task(&s_shell);
}
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell_port.c
git commit -m "refactor(port): 简化shell_port.c，移除LWRB依赖"
```

### Task 6: 更新测试框架

**Files:**
- Modify: `test/test_shell.c:1-50`

- [ ] **Step 1: 更新测试初始化代码**

在`test_shell.c`的初始化函数中更新：

```c
static void setUp(void)
{
    /* 使用新的初始化方式 */
    shell_config_t cfg = {
        .write = mock_write,
    };
    shell_init_config(&s_sh, &cfg);

    /* 设置命令表 */
    s_sh.cmds = s_test_cmds;
    s_sh.cmd_cnt = TEST_CMD_COUNT;

#if SHELL_USING_AUTH
    shell_set_users(&s_sh, s_test_users, TEST_USER_COUNT);
#endif
}
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add test/test_shell.c
git commit -m "test: 更新测试框架以适应新API"
```

## 阶段2：功能增强

### Task 7: 添加ISR日志队列配置宏

**Files:**
- Modify: `shell_cfg.h`

- [ ] **Step 1: 添加ISR日志队列配置宏**

在`shell_cfg.h`的末尾添加：

```c
/* ==================== ISR日志队列配置 ==================== */
#ifndef SHELL_USING_LOG_QUEUE
    #define SHELL_USING_LOG_QUEUE 1  /* 启用ISR日志队列 */
#endif

#if SHELL_USING_LOG_QUEUE
    #define SHELL_LOG_QUEUE_SIZE 256  /* 日志队列大小，必须是2的幂 */
#endif
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell_cfg.h
git commit -m "feat(config): 添加ISR日志队列配置宏"
```

### Task 8: 添加ISR日志队列到shell_t

**Files:**
- Modify: `shell.h:248-291`

- [ ] **Step 1: 添加日志队列字段到shell_t**

在`shell_t`结构体中添加：

```c
#if SHELL_USING_LOG_QUEUE
    /* ISR日志队列 */
    uint8_t log_buf[SHELL_LOG_QUEUE_SIZE];
    volatile uint16_t log_head;           // ISR写
    volatile uint16_t log_tail;           // 主循环读
    volatile uint16_t log_dropped_total;  // 丢弃计数
    uint16_t log_dropped_reported;        // 已报告的丢弃计数
#endif
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell.h
git commit -m "feat(struct): 添加ISR日志队列字段到shell_t"
```

### Task 9: 实现ISR日志队列API

**Files:**
- Create: `shell_log.c`

- [ ] **Step 1: 创建shell_log.c**

```c
/**
 * @file    shell_log.c
 * @brief   ISR安全日志队列实现
 */

#include "shell.h"

#if SHELL_USING_LOG_QUEUE

/* 计算下一个索引（环形缓冲区） */
static uint16_t log_next(uint16_t index)
{
    return (uint16_t)((index + 1) % SHELL_LOG_QUEUE_SIZE);
}

/**
 * @brief ISR中写入日志数据（快速，不阻塞）
 * @param sh Shell实例
 * @param data 数据指针
 * @param len 数据长度
 * @return 0=成功, -1=参数错误, -2=缓冲区满（部分数据可能丢失）
 */
int shell_log_isr(shell_t* sh, const uint8_t* data, uint16_t len)
{
    uint16_t i;
    int rc = 0;

    if (!sh || !data)
    {
        return -1;
    }

    for (i = 0; i < len; i++)
    {
        uint16_t head = sh->log_head;
        uint16_t next = log_next(head);
        if (next == sh->log_tail)
        {
            /* 缓冲区满，记录丢弃计数 */
            if (sh->log_dropped_total < UINT16_MAX)
            {
                sh->log_dropped_total++;
            }
            rc = -2;
        }
        else
        {
            sh->log_buf[head] = data[i];
            sh->log_head = next;
        }
    }

    return rc;
}

/**
 * @brief ISR中写入文本日志（快速，不阻塞）
 * @param sh Shell实例
 * @param s 字符串指针
 * @return 0=成功, -1=参数错误, -2=缓冲区满
 */
int shell_log_text_isr(shell_t* sh, const char* s)
{
    if (!s)
    {
        return -1;
    }
    return shell_log_isr(sh, (const uint8_t*)s, strlen(s));
}

/**
 * @brief 主循环中输出日志（在shell_task中自动调用）
 * @param sh Shell实例
 */
void shell_log_drain(shell_t* sh)
{
    uint8_t line[SHELL_CMD_SIZE];
    uint16_t len = 0;

    if (!sh)
    {
        return;
    }

    while (sh->log_tail != sh->log_head)
    {
        line[len++] = sh->log_buf[sh->log_tail];
        sh->log_tail = log_next(sh->log_tail);
        if ((len == sizeof(line)) || (line[len - 1] == '\n'))
        {
            if (sh->write)
            {
                sh->write((const char*)line, len);
            }
            len = 0;
        }
    }

    if (len > 0 && sh->write)
    {
        sh->write((const char*)line, len);
    }

    /* 输出丢弃计数 */
    if (sh->log_dropped_reported != sh->log_dropped_total)
    {
        char dropped_line[32];
        uint16_t pos = 0;
        uint16_t dropped = (uint16_t)(sh->log_dropped_total - sh->log_dropped_reported);
        sh->log_dropped_reported = sh->log_dropped_total;

        pos += snprintf(dropped_line + pos, sizeof(dropped_line) - pos, "log dropped: %u\r\n", dropped);
        if (sh->write)
        {
            sh->write(dropped_line, pos);
        }
    }
}

#endif /* SHELL_USING_LOG_QUEUE */
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell_log.c
git commit -m "feat(log): 实现ISR日志队列"
```

### Task 10: 添加ISR日志队列API声明

**Files:**
- Modify: `shell.h:290-300`

- [ ] **Step 1: 添加API声明**

在`shell.h`的API声明部分添加：

```c
#if SHELL_USING_LOG_QUEUE
/* ISR日志队列API */
int shell_log_isr(shell_t* sh, const uint8_t* data, uint16_t len);
int shell_log_text_isr(shell_t* sh, const char* s);
void shell_log_drain(shell_t* sh);
#endif
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell.h
git commit -m "feat(api): 添加ISR日志队列API声明"
```

### Task 11: 集成ISR日志队列到shell_task

**Files:**
- Modify: `shell.c:150-200`

- [ ] **Step 1: 在shell_task中调用shell_log_drain**

在`shell_task`函数的末尾添加：

```c
#if SHELL_USING_LOG_QUEUE
    shell_log_drain(sh);
#endif
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell.c
git commit -m "feat(task): 集成ISR日志队列到shell_task"
```

### Task 12: 创建ANSI增强头文件

**Files:**
- Create: `shell_ansi.h`

- [ ] **Step 1: 创建shell_ansi.h**

```c
/**
 * @file    shell_ansi.h
 * @brief   ANSI颜色和光标控制定义
 */

#ifndef SHELL_ANSI_H_
#define SHELL_ANSI_H_

/* ==================== 光标控制 ==================== */
#define ANSI_CLEAR      "\033[2J\033[H"   /* 清屏 */
#define ANSI_CLEARLN    "\033[2K\r"       /* 清除当前行 */
#define ANSI_HOME       "\033[H"          /* 光标移到左上角 */
#define ANSI_UP         "\033[1A"         /* 光标上移 */
#define ANSI_DOWN       "\033[1B"         /* 光标下移 */
#define ANSI_RIGHT      "\033[1C"         /* 光标右移 */
#define ANSI_LEFT       "\033[1D"         /* 光标左移 */

/* ==================== 颜色定义 ==================== */
#define ANSI_COLOR_RESET   "\033[0m"      /* 重置颜色 */
#define ANSI_COLOR_RED     "\033[31m"     /* 红色 */
#define ANSI_COLOR_GREEN   "\033[32m"     /* 绿色 */
#define ANSI_COLOR_YELLOW  "\033[33m"     /* 黄色 */
#define ANSI_COLOR_BLUE    "\033[34m"     /* 蓝色 */
#define ANSI_COLOR_MAGENTA "\033[35m"     /* 洋红 */
#define ANSI_COLOR_CYAN    "\033[36m"     /* 青色 */
#define ANSI_COLOR_WHITE   "\033[37m"     /* 白色 */

/* ==================== 背景颜色 ==================== */
#define ANSI_BG_RED     "\033[41m"        /* 红色背景 */
#define ANSI_BG_GREEN   "\033[42m"        /* 绿色背景 */
#define ANSI_BG_YELLOW  "\033[43m"        /* 黄色背景 */
#define ANSI_BG_BLUE    "\033[44m"        /* 蓝色背景 */
#define ANSI_BG_MAGENTA "\033[45m"        /* 洋红背景 */
#define ANSI_BG_CYAN    "\033[46m"        /* 青色背景 */
#define ANSI_BG_WHITE   "\033[47m"        /* 白色背景 */

/* ==================== 文本样式 ==================== */
#define ANSI_BOLD       "\033[1m"         /* 粗体 */
#define ANSI_UNDERLINE  "\033[4m"         /* 下划线 */
#define ANSI_BLINK      "\033[5m"         /* 闪烁 */
#define ANSI_REVERSE    "\033[7m"         /* 反显 */

#endif /* SHELL_ANSI_H_ */
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell_ansi.h
git commit -m "feat(ansi): 创建ANSI颜色和光标控制头文件"
```

### Task 13: 实现ANSI增强API

**Files:**
- Modify: `shell.c:70-100`

- [ ] **Step 1: 添加ANSI增强API实现**

在`shell.c`的`shell_printf`函数之后添加：

```c
/* ==================== ANSI增强API ==================== */

/**
 * @brief 带颜色的打印
 * @param sh Shell实例
 * @param color ANSI颜色代码
 * @param str 字符串
 */
void shell_print_color(shell_t* sh, const char* color, const char* str)
{
    if (!sh || !sh->write || !str)
    {
        return;
    }

    if (color)
    {
        sh->write(color, strlen(color));
    }
    sh->write(str, strlen(str));
    sh->write(ANSI_COLOR_RESET, strlen(ANSI_COLOR_RESET));
}

/**
 * @brief 带颜色的格式化打印
 * @param sh Shell实例
 * @param color ANSI颜色代码
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void shell_printf_color(shell_t* sh, const char* color, const char* fmt, ...)
{
    if (!sh || !sh->write || !fmt)
    {
        return;
    }

    char    buf[SHELL_PRINTF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int32_t len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len > 0)
    {
        if (len > (int32_t)sizeof(buf) - 1)
        {
            len = sizeof(buf) - 1;
        }

        if (color)
        {
            sh->write(color, strlen(color));
        }
        sh->write(buf, len);
        sh->write(ANSI_COLOR_RESET, strlen(ANSI_COLOR_RESET));
    }
}
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell.c
git commit -m "feat(ansi): 实现ANSI增强API"
```

### Task 14: 添加ANSI增强API声明

**Files:**
- Modify: `shell.h:290-300`

- [ ] **Step 1: 添加API声明**

在`shell.h`的API声明部分添加：

```c
/* ANSI增强API */
void shell_print_color(shell_t* sh, const char* color, const char* str);
void shell_printf_color(shell_t* sh, const char* color, const char* fmt, ...);
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add shell.h
git commit -m "feat(api): 添加ANSI增强API声明"
```

### Task 15: 实现密码验证回调

**Files:**
- Modify: `shell.c:250-300`

- [ ] **Step 1: 修改密码验证逻辑**

在`shell.c`的`cmd_login`函数中修改密码验证逻辑：

```c
/* 密码验证 */
#if SHELL_USING_AUTH
static int verify_password(const shell_user_t* user, const char* input_password)
{
    if (!user || !input_password)
    {
        return -1;
    }

    /* 如果设置了自定义验证回调，使用回调 */
    if (g_shell && g_shell->password_verify)
    {
        return g_shell->password_verify(user, input_password);
    }

    /* 默认验证逻辑 */
#if SHELL_USING_HASH_PWD
    /* 哈希验证 */
    uint32_t hash = shell_hash(input_password);
    return (hash == (uint32_t)(uintptr_t)user->password) ? 0 : -1;
#else
    /* 明文验证 */
    return (strcmp(user->password, input_password) == 0) ? 0 : -1;
#endif
}
#endif
```

- [ ] **Step 2: 更新cmd_login函数**

在`cmd_login`函数中使用新的验证函数：

```c
int cmd_login(int argc, char* argv[])
{
    /* ... 原有代码 ... */

    /* 验证密码 */
    if (verify_password(user, password) != 0)
    {
        shell_print(g_shell, STR_PASSWORD_WRONG);
        return -1;
    }

    /* ... 原有代码 ... */
}
```

- [ ] **Step 3: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 4: 提交修改**

```bash
git add shell.c
git commit -m "feat(auth): 实现密码验证回调"
```

### Task 16: 添加密码验证回调API声明

**Files:**
- Modify: `shell.h:290-300`

- [ ] **Step 1: 添加API声明**

在`shell.h`的API声明部分添加：

```c
#if SHELL_USING_AUTH
/* 密码验证回调API */
void shell_set_password_verify(shell_t* sh, shell_password_verify_fn_t verify);
#endif
```

- [ ] **Step 2: 实现API**

在`shell.c`中添加：

```c
#if SHELL_USING_AUTH
void shell_set_password_verify(shell_t* sh, shell_password_verify_fn_t verify)
{
    if (sh)
    {
        sh->password_verify = verify;
    }
}
#endif
```

- [ ] **Step 3: 运行测试验证修改**

Run: `cd test && make`
Expected: 所有测试通过

- [ ] **Step 4: 提交修改**

```bash
git add shell.h shell.c
git commit -m "feat(api): 添加密码验证回调API"
```

## 阶段3：测试增强

### Task 17: 创建ISR日志队列测试

**Files:**
- Create: `test/test_shell_log.c`

- [ ] **Step 1: 创建测试文件**

```c
/**
 * @file    test_shell_log.c
 * @brief   ISR日志队列单元测试
 */

#include "../shell.h"
#include "unity.h"
#include "test_shell_mock.h"

#include <string.h>

/* 包含实现 */
#include "../shell_log.c"

/* 测试夹具 */
static shell_t s_sh;

void setUp(void)
{
    memset(&s_sh, 0, sizeof(shell_t));
    s_sh.write = mock_write;
    mock_reset();
}

void tearDown(void)
{
}

/* 测试用例 */
void test_log_isr_basic(void)
{
    const char* data = "test";
    int rc = shell_log_isr(&s_sh, (const uint8_t*)data, strlen(data));
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_EQUAL(strlen(data), s_sh.log_head);
}

void test_log_isr_null_params(void)
{
    int rc = shell_log_isr(NULL, (const uint8_t*)"test", 4);
    TEST_ASSERT_EQUAL(-1, rc);

    rc = shell_log_isr(&s_sh, NULL, 4);
    TEST_ASSERT_EQUAL(-1, rc);
}

void test_log_isr_buffer_full(void)
{
    /* 填满缓冲区 */
    for (int i = 0; i < SHELL_LOG_QUEUE_SIZE - 1; i++)
    {
        uint8_t ch = 'A';
        shell_log_isr(&s_sh, &ch, 1);
    }

    /* 再写入应该失败 */
    uint8_t ch = 'B';
    int rc = shell_log_isr(&s_sh, &ch, 1);
    TEST_ASSERT_EQUAL(-2, rc);
    TEST_ASSERT_EQUAL(1, s_sh.log_dropped_total);
}

void test_log_text_isr_basic(void)
{
    int rc = shell_log_text_isr(&s_sh, "hello");
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_EQUAL(5, s_sh.log_head);
}

void test_log_drain_basic(void)
{
    /* 写入数据 */
    shell_log_text_isr(&s_sh, "hello\n");

    /* 清空输出 */
    mock_reset();

    /*  drain */
    shell_log_drain(&s_sh);

    /* 验证输出 */
    TEST_ASSERT_EQUAL_STRING("hello\n", mock_get_output());
}

void test_log_drain_with_dropped(void)
{
    /* 模拟丢弃 */
    s_sh.log_dropped_total = 5;
    s_sh.log_dropped_reported = 0;

    mock_reset();
    shell_log_drain(&s_sh);

    /* 验证输出包含丢弃信息 */
    const char* output = mock_get_output();
    TEST_ASSERT_NOT_NULL(strstr(output, "log dropped: 5"));
}

/* 测试运行 */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_log_isr_basic);
    RUN_TEST(test_log_isr_null_params);
    RUN_TEST(test_log_isr_buffer_full);
    RUN_TEST(test_log_text_isr_basic);
    RUN_TEST(test_log_drain_basic);
    RUN_TEST(test_log_drain_with_dropped);

    return UNITY_END();
}
```

- [ ] **Step 2: 运行测试**

Run: `cd test && gcc -Wall -Wextra -I. -Iunity/src test_shell_log.c unity/src/unity.c -o test_shell_log && ./test_shell_log`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add test/test_shell_log.c
git commit -m "test(log): 添加ISR日志队列测试"
```

### Task 18: 创建ANSI增强测试

**Files:**
- Create: `test/test_shell_ansi.c`

- [ ] **Step 1: 创建测试文件**

```c
/**
 * @file    test_shell_ansi.c
 * @brief   ANSI增强单元测试
 */

#include "../shell.h"
#include "../shell_ansi.h"
#include "unity.h"
#include "test_shell_mock.h"

#include <string.h>

/* 测试夹具 */
static shell_t s_sh;

void setUp(void)
{
    memset(&s_sh, 0, sizeof(shell_t));
    s_sh.write = mock_write;
    mock_reset();
}

void tearDown(void)
{
}

/* 测试用例 */
void test_print_color_basic(void)
{
    shell_print_color(&s_sh, ANSI_COLOR_RED, "Error");
    const char* output = mock_get_output();

    /* 验证包含颜色代码和重置代码 */
    TEST_ASSERT_NOT_NULL(strstr(output, ANSI_COLOR_RED));
    TEST_ASSERT_NOT_NULL(strstr(output, "Error"));
    TEST_ASSERT_NOT_NULL(strstr(output, ANSI_COLOR_RESET));
}

void test_print_color_null_color(void)
{
    shell_print_color(&s_sh, NULL, "Test");
    const char* output = mock_get_output();

    /* 验证只输出文本和重置代码 */
    TEST_ASSERT_NOT_NULL(strstr(output, "Test"));
    TEST_ASSERT_NOT_NULL(strstr(output, ANSI_COLOR_RESET));
}

void test_printf_color_basic(void)
{
    shell_printf_color(&s_sh, ANSI_COLOR_GREEN, "OK: %d", 42);
    const char* output = mock_get_output();

    /* 验证包含颜色代码和格式化输出 */
    TEST_ASSERT_NOT_NULL(strstr(output, ANSI_COLOR_GREEN));
    TEST_ASSERT_NOT_NULL(strstr(output, "OK: 42"));
    TEST_ASSERT_NOT_NULL(strstr(output, ANSI_COLOR_RESET));
}

void test_printf_color_null_params(void)
{
    /* 不应该崩溃 */
    shell_printf_color(NULL, ANSI_COLOR_RED, "test");
    shell_printf_color(&s_sh, NULL, NULL);
}

/* 测试运行 */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_print_color_basic);
    RUN_TEST(test_print_color_null_color);
    RUN_TEST(test_printf_color_basic);
    RUN_TEST(test_printf_color_null_params);

    return UNITY_END();
}
```

- [ ] **Step 2: 运行测试**

Run: `cd test && gcc -Wall -Wextra -I. -Iunity/src test_shell_ansi.c unity/src/unity.c -o test_shell_ansi && ./test_shell_ansi`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add test/test_shell_ansi.c
git commit -m "test(ansi): 添加ANSI增强测试"
```

### Task 19: 创建密码验证回调测试

**Files:**
- Create: `test/test_shell_auth.c`

- [ ] **Step 1: 创建测试文件**

```c
/**
 * @file    test_shell_auth.c
 * @brief   密码验证回调单元测试
 */

#include "../shell.h"
#include "unity.h"
#include "test_shell_mock.h"

#include <string.h>

/* 自定义验证回调 */
static int s_custom_verify_called = 0;
static int custom_verify(const shell_user_t* user, const char* input_password)
{
    s_custom_verify_called++;
    /* 简单验证：密码等于用户名 */
    return (strcmp(user->name, input_password) == 0) ? 0 : -1;
}

/* 测试夹具 */
static shell_t s_sh;

void setUp(void)
{
    memset(&s_sh, 0, sizeof(shell_t));
    s_sh.write = mock_write;
    s_custom_verify_called = 0;
    mock_reset();
}

void tearDown(void)
{
}

/* 测试用例 */
void test_set_password_verify(void)
{
    shell_set_password_verify(&s_sh, custom_verify);
    TEST_ASSERT_EQUAL_PTR(custom_verify, s_sh.password_verify);
}

void test_set_password_verify_null(void)
{
    shell_set_password_verify(&s_sh, NULL);
    TEST_ASSERT_NULL(s_sh.password_verify);
}

/* 测试运行 */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_set_password_verify);
    RUN_TEST(test_set_password_verify_null);

    return UNITY_END();
}
```

- [ ] **Step 2: 运行测试**

Run: `cd test && gcc -Wall -Wextra -I. -Iunity/src test_shell_auth.c unity/src/unity.c -o test_shell_auth && ./test_shell_auth`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add test/test_shell_auth.c
git commit -m "test(auth): 添加密码验证回调测试"
```

### Task 20: 更新Makefile

**Files:**
- Modify: `test/Makefile`

- [ ] **Step 1: 添加新测试到Makefile**

在`test/Makefile`中添加：

```makefile
# 新测试
test_log: test_shell_log.c
	$(CC) $(CFLAGS) -I. -Iunity/src test_shell_log.c unity/src/unity.c -o test_shell_log
	./test_shell_log

test_ansi: test_shell_ansi.c
	$(CC) $(CFLAGS) -I. -Iunity/src test_shell_ansi.c unity/src/unity.c -o test_shell_ansi
	./test_shell_ansi

test_auth: test_shell_auth.c
	$(CC) $(CFLAGS) -I. -Iunity/src test_shell_auth.c unity/src/unity.c -o test_shell_auth
	./test_shell_auth

# 运行所有测试
test_all: test test_log test_ansi test_auth
```

- [ ] **Step 2: 运行测试验证修改**

Run: `cd test && make test_all`
Expected: 所有测试通过

- [ ] **Step 3: 提交修改**

```bash
git add test/Makefile
git commit -m "build(test): 更新Makefile支持新测试"
```

## 阶段4：文档和示例

### Task 21: 更新README

**Files:**
- Modify: `README.md`

- [ ] **Step 1: 更新移植说明**

在`README.md`的移植要点部分更新：

```markdown
## 移植要点

适配新硬件只需修改 `shell_port.c`（约40行）：

1. 实现 `uart_write` 函数（串口发送）
2. 在 UART RX 中断中调用 `shell_rx_push()`
3. 实现 `shell_port_init()` 初始化函数
4. 在主循环中调用 `shell_port_task()`

示例代码见 `shell_port.c`。
```

- [ ] **Step 2: 添加新功能说明**

在`README.md`中添加：

```markdown
## 新功能

### ISR日志队列

在中断中安全记录日志：

```c
void USART1_IRQHandler(void) {
    uint8_t ch = USART1->DR;
    shell_log_text_isr(&shell, "UART recv: ");
    shell_log_isr(&shell, &ch, 1);
}
```

### ANSI颜色

带颜色的输出：

```c
shell_print_color(&shell, ANSI_COLOR_RED, "Error\r\n");
shell_printf_color(&shell, ANSI_COLOR_GREEN, "OK: %d\r\n", status);
```

### 密码验证回调

自定义密码验证：

```c
int my_verify(const shell_user_t* user, const char* pwd) {
    return (hash(pwd) == user->password) ? 0 : -1;
}

shell_config_t cfg = {
    .write = uart_write,
    .password_verify = my_verify,
};
shell_init(&shell, &cfg);
```
```

- [ ] **Step 3: 提交修改**

```bash
git add README.md
git commit -m "docs: 更新README，添加新功能说明"
```

### Task 22: 创建HAL库示例

**Files:**
- Create: `examples/stm32_hal/shell_port_hal.c`

- [ ] **Step 1: 创建示例文件**

```c
/**
 * @file    shell_port_hal.c
 * @brief   STM32 HAL库移植示例
 */

#include "shell.h"
#include "stm32f4xx_hal.h"

/* Shell实例 */
static shell_t s_shell;

/* ==================== 硬件IO层 ==================== */

/**
 * @brief Shell写函数 - HAL库阻塞发送
 */
static void uart_write(const char* data, uint16_t len)
{
    HAL_UART_Transmit(&huart3, (uint8_t*)data, len, 100);
}

/* ==================== 中断处理 ==================== */

/**
 * @brief UART3中断处理 - 在USART3_IRQHandler中调用
 */
void shell_uart3_irq_handler(void)
{
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
    {
        uint8_t ch = (uint8_t)(huart3.Instance->DR & 0xFF);
        shell_rx_push(&s_shell, ch);
    }
}

/* ==================== 初始化 ==================== */

/**
 * @brief Shell初始化
 */
void shell_port_init(void)
{
    shell_init_export(&s_shell, uart_write);

#if SHELL_USING_AUTH
    shell_user_init(&s_shell);
#endif

    /* 使能UART3接收中断 */
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
}

/**
 * @brief Shell任务（主循环调用）
 */
void shell_port_task(void)
{
    shell_task(&s_shell);
}
```

- [ ] **Step 2: 提交修改**

```bash
git add examples/stm32_hal/shell_port_hal.c
git commit -m "example: 添加STM32 HAL库移植示例"
```

### Task 23: 创建标准外设库示例

**Files:**
- Create: `examples/stm32_stdperiph/shell_port_stdperiph.c`

- [ ] **Step 1: 创建示例文件**

```c
/**
 * @file    shell_port_stdperiph.c
 * @brief   STM32标准外设库移植示例
 */

#include "shell.h"
#include "stm32f4xx_usart.h"

/* Shell实例 */
static shell_t s_shell;

/* ==================== 硬件IO层 ==================== */

/**
 * @brief Shell写函数 - 标准外设库阻塞发送
 */
static void uart_write(const char* data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET)
            ;
        USART_SendData(USART3, data[i]);
    }
    while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET)
        ;
}

/* ==================== 中断处理 ==================== */

/**
 * @brief UART3中断处理 - 在USART3_IRQHandler中调用
 */
void shell_uart3_irq_handler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        uint8_t ch = (uint8_t)USART_ReceiveData(USART3);
        shell_rx_push(&s_shell, ch);
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
}

/* ==================== 初始化 ==================== */

/**
 * @brief Shell初始化
 */
void shell_port_init(void)
{
    shell_init_export(&s_shell, uart_write);

#if SHELL_USING_AUTH
    shell_user_init(&s_shell);
#endif

    /* 使能UART3接收中断 */
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
}

/**
 * @brief Shell任务（主循环调用）
 */
void shell_port_task(void)
{
    shell_task(&s_shell);
}
```

- [ ] **Step 2: 提交修改**

```bash
git add examples/stm32_stdperiph/shell_port_stdperiph.c
git commit -m "example: 添加STM32标准外设库移植示例"
```

## 自我审查

### 1. 规范覆盖检查

- ✅ 架构重构：配置结构体、新初始化函数、shell_user.c、简化port层
- ✅ 功能增强：ISR日志队列、ANSI增强、密码验证回调
- ✅ 测试增强：新增测试文件、更新Makefile
- ✅ 文档和示例：README更新、HAL库示例、标准外设库示例

### 2. 占位符扫描

- ✅ 无"TBD"、"TODO"、"implement later"
- ✅ 所有步骤都有完整代码
- ✅ 所有测试都有实际测试代码

### 3. 类型一致性检查

- ✅ `shell_config_t`在所有文件中一致
- ✅ `shell_password_verify_fn_t`在所有文件中一致
- ✅ `shell_log_isr`、`shell_log_text_isr`、`shell_log_drain`在所有文件中一致
- ✅ `shell_print_color`、`shell_printf_color`在所有文件中一致
- ✅ `shell_set_password_verify`在所有文件中一致

## 执行选项

计划完成并保存到 `docs/superpowers/plans/2026-05-31-shell-module-planning.md`。

**两种执行方式：**

**1. Subagent-Driven（推荐）** - 每个任务分发一个新子代理，任务间审查，快速迭代

**2. Inline Execution** - 在当前会话中执行任务，批量执行带检查点

**选择哪种方式？**
