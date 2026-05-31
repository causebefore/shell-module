# Shell模块 Unity 单元测试 Makefile
# 使用方法:
#   make          - 编译并运行主测试
#   make build    - 仅编译主测试
#   make run      - 运行主测试
#   make test_log  - 编译并运行ISR日志队列测试
#   make test_ansi - 编译并运行ANSI增强测试
#   make test_auth - 编译并运行密码验证回调测试
#   make test_all  - 运行所有测试
#   make clean    - 清理
#   make unity    - 下载Unity框架

CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-unused-parameter -std=c99 -I. -Iunity/src -I..

# 跨平台可执行文件后缀
ifeq ($(OS),Windows_NT)
    TARGET      = test_shell.exe
    TARGET_LOG  = test_shell_log.exe
    TARGET_ANSI = test_shell_ansi.exe
    TARGET_AUTH = test_shell_auth.exe
else
    TARGET      = test_shell
    TARGET_LOG  = test_shell_log
    TARGET_ANSI = test_shell_ansi
    TARGET_AUTH = test_shell_auth
endif

SRCS    = test_shell.c test_shell_mock.c unity/src/unity.c

.PHONY: all build run clean unity test_log test_ansi test_auth test_all

all: build run

build: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

# ISR日志队列测试
test_log: test_shell_log.c test_shell_mock.c unity/src/unity.c
	$(CC) $(CFLAGS) -o $(TARGET_LOG) $^
	./$(TARGET_LOG)

# ANSI增强测试
test_ansi: test_shell_ansi.c test_shell_mock.c unity/src/unity.c
	$(CC) $(CFLAGS) -o $(TARGET_ANSI) $^
	./$(TARGET_ANSI)

# 密码验证回调测试
test_auth: test_shell_auth.c test_shell_mock.c unity/src/unity.c
	$(CC) $(CFLAGS) -o $(TARGET_AUTH) $^
	./$(TARGET_AUTH)

# 运行所有测试
test_all: all test_log test_ansi test_auth

clean:
	rm -f $(TARGET) $(TARGET_LOG) $(TARGET_ANSI) $(TARGET_AUTH) *.o

# 下载 Unity 测试框架
unity:
	@if [ ! -d "unity" ]; then \
		echo "Downloading Unity..."; \
		git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git unity; \
	else \
		echo "Unity already exists."; \
	fi
