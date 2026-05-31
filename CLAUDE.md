# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 构建与测试

```bash
# 主机端单元测试（需 GCC）
cd test
make unity    # 首次：克隆 Unity 测试框架
make          # 编译并运行全部测试（约 82 条）
make clean    # 清理

# 单独编译
make build    # 仅编译，不运行
make run      # 仅运行已编译的测试
```

CI 在 push/PR 到 `master` 时自动触发（仅 .c/.h/test 路径变更时），复用外部工作流 `causebefore/embedded-ci/.github/workflows/embedded-ci.yml@v1`，执行 cppcheck 静态分析、ARM 交叉编译和 clang-format 格式检查。

## 架构

这是一个面向 ARM Cortex-M 裸机环境的轻量级嵌入式 CLI 框架，通过 UART 提供交互式命令行。

| 文件 | 职责 |
|---|---|
| `shell.h` / `shell.c` | 核心实现：命令解析、行编辑、ESC序列、历史记录、Tab补全、变量读写、权限认证、透传模式 |
| `shell_cfg.h` | 编译时功能开关与参数配置（所有宏可通过 `-D` 覆盖） |
| `shell_port.h` / `shell_port.c` | 移植层：硬件 IO 适配（示例基于 STM32 HAL + LWRB）、用户/命令/变量注册 |

**核心设计：**

- **命令自动注册**：`SHELL_EXPORT_CMD` 宏将命令放入链接器 section（`.shell_cmd`），运行时通过 `__shell_cmd_start`/`__shell_cmd_end` 边界符号收集，无需集中式命令表。变量通过 `SHELL_EXPORT_VAR` 同理。
- **零动态内存**：`shell_t` 控制块和所有缓冲区均为静态分配。
- **ISR 安全**：内置环形缓冲区（`rx_buf`）支持中断中 `shell_rx_push()` 写入，主循环 `shell_task()` 读取，使用内存屏障确保顺序。
- **初始化流程**：`my_shell_init()` → `shell_init()` 绑定 write/read 回调 → 主循环 `my_shell_task()` → `shell_task()` 读取字符 → `shell_input()` 逐字符处理（行编辑/ESC序列/补全/执行）。

## 测试架构

测试位于 `test/`，使用 Unity 框架。通过 `#include "../shell.c"` 将核心实现内联编译到 `test_shell.c` 中，并覆盖 `SHELL_VAR_LIST`/`SHELL_VAR_COUNT` 宏用静态数组替代链接器 section 机制。

Mock 模块（`test_shell_mock.c/.h`）提供 `mock_write` 捕获输出、`mock_read_set` 注入输入、`test_cmd_record` 验证命令参数解析。

## 代码规范

- **类型**：使用 `stdint.h` 固定宽度类型（`uint8_t`、`int32_t` 等）。命令回调签名 `int func(int argc, char* argv[])` 是唯一允许使用裸 `int` 的例外。
- **内存**：禁止 `malloc`/`free`，所有内存静态分配。
- **Volatile**：中断共享变量和硬件寄存器访问必须使用 `volatile`。
- **命名**：文件作用域静态变量前缀 `s_`，全局变量前缀 `g_`。
- **字符串**：输出字符串集中定义为 `STR_xxx` 宏，便于定制/翻译。
- **格式化**：Allman 大括号风格（大括号独占一行），4 空格缩进，120 列。`.clang-format` 不设置 `Language: Cpp` 以避免 `.h` 文件被误识别为 Objective-C。
- **头文件卫士**：统一使用末尾 `_` 命名（如 `SHELL_PORT_H_`）。
- **注释**：中文注释；公共 API 使用 Doxygen 风格。

## 移植要点

适配新硬件只需修改 `shell_port.c`：
1. 实现 `write(const char* data, uint16_t len)` 回调（串口 TX）
2. 实现 `read(char* data, uint16_t len)` 回调，或使用内置环形缓冲区 + 在 UART RX 中断中调用 `shell_rx_push()`
3. 配置用户表（若启用 `SHELL_USING_AUTH`）
4. 使用 `SHELL_EXPORT_CMD`/`SHELL_EXPORT_VAR` 注册命令和变量
5. 若使用段导出（`SHELL_USING_CMD_EXPORT=1`），需在链接脚本中添加 `.shell_cmd` 和 `.shell_var` 段
6. 主循环中周期性调用 `shell_task()`
