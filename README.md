# Shell

轻量级嵌入式命令行界面，适用于调试和交互控制。

## 特性

- 命令注册与执行 (支持宏导出)
- 参数解析 (空格分隔)
- 历史记录 (上下箭头)
- Tab 补全 (命令名/参数)
- 变量读写 (int/uint/float/bool/string)
- 用户认证与权限控制
- 透传模式
- ANSI 颜色输出
- ISR 安全日志队列
- 环形接收缓冲区 (中断安全)

## 移植说明

### 最小移植

推荐通过 `shell_port_backend_t` 接入底层传输，shell 核心只负责命令解析：

```c
static void console_write(void* ctx, const char* data, uint16_t len)
{
    (void) ctx;
    for (uint16_t i = 0; i < len; i++)
    {
        /* 实际项目应加入超时，避免外设异常时死等 */
        while (!(USART1->SR & USART_SR_TXE));
        USART1->DR = data[i];
    }
}

static int console_start(void* ctx)
{
    (void) ctx;
    USART1->CR1 |= USART_CR1_RXNEIE;
    return 0;
}

static const shell_port_backend_t backend = {
    .ctx = NULL,
    .write = console_write,
    .critical_enter = NULL,
    .critical_exit = NULL,
    .start = console_start,
};

shell_port_init(&backend);
```

### 主循环集成

```c
while (1) { shell_port_task(); }
```

中断接收模式下，在 ISR 中调用：

```c
void USART1_IRQHandler(void)
{
    if (USART1->SR & USART_SR_RXNE)
    {
        uint8_t ch = USART1->DR;
        shell_port_rx_from_isr(ch);
    }
}
```

也可通过 `read` 回调实现轮询接收（参见示例）。

### 移植示例

| 示例                                   | 环境               |
| -------------------------------------- | ------------------ |
| `examples/stm32_hal/`                  | STM32 HAL 库       |
| `examples/stm32_stdperiph/`            | STM32 标准外设库   |

## 新功能说明

### ISR 安全日志队列

在中断中安全输出日志，由主循环 drain 输出：

```c
// 中断中调用
shell_log_text_isr(&sh, "ISR event!\r\n");

// 主循环中 shell_task 会自动调用 shell_log_drain
// 也可手动调用
shell_log_drain(&sh);
```

配置 (`shell_cfg.h`):
- `SHELL_USING_LOG_QUEUE`: 启用/禁用 (默认 1)
- `SHELL_LOG_QUEUE_SIZE`: 队列大小，必须是 2 的幂 (默认 256)

### ANSI 颜色输出

```c
#include "shell_ansi.h"

// 带颜色输出
shell_print_color(&sh, ANSI_COLOR_RED, "Error!\r\n");
shell_printf_color(&sh, ANSI_COLOR_GREEN, "Value: %d\r\n", 42);
```

### 密码验证回调

密码验证必须由 `password_verify` 接管。未设置回调时，所有用户登录都会失败。

```c
static int my_verify(const shell_user_t* user, const char* pwd)
{
    // 自定义验证逻辑
    return check_password(user->name, pwd) ? 0 : -1;
}

shell_set_password_verify(&sh, my_verify);

// 清除验证回调，登录将失败
shell_set_password_verify(&sh, NULL);
```

## 配置选项

所有配置在 `shell_cfg.h` 中，可通过编译器 `-D` 选项覆盖：

| 宏                      | 说明              | 默认值 |
| ----------------------- | ----------------- | ------ |
| `SHELL_CMD_SIZE`        | 命令缓冲区大小    | 128    |
| `SHELL_ARG_MAX`         | 最大参数数量      | 16     |
| `SHELL_RX_BUF_SIZE`    | 接收环形缓冲区    | 64     |
| `SHELL_USING_HISTORY`  | 历史记录          | 1      |
| `SHELL_USING_COMPLETION`| Tab 补全         | 1      |
| `SHELL_USING_VAR`      | 变量读写          | 1      |
| `SHELL_USING_AUTH`     | 用户认证          | 1      |
| `SHELL_USING_PASSTHROUGH`| 透传模式        | 1      |
| `SHELL_USING_LOG_QUEUE`| ISR 日志队列      | 1      |

## 命令导出

在任意 `.c` 文件中注册命令：

```c
static int cmd_hello(int argc, char* argv[])
{
    shell_printf(g_shell, "Hello, %s!\r\n", argc > 1 ? argv[1] : "World");
    return 0;
}
SHELL_EXPORT_CMD(hello, "Say hello", cmd_hello, SHELL_PERM_NONE);
```

Keil 工程需要链接脚本提供 `SHELL_CMD` / `SHELL_VAR` execution region。

## 运行测试

```bash
cd test
make unity      # 首次运行需下载 Unity 框架
make            # 运行主测试
make test_all   # 运行所有测试
```
