# Shell模块 Unity 单元测试 Makefile
# 使用方法:
#   make          - 编译并运行测试
#   make build    - 仅编译
#   make run      - 运行测试
#   make clean    - 清理
#   make unity    - 下载Unity框架

CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-unused-parameter -std=c99 -I. -Iunity/src -I..

# 跨平台可执行文件后缀
ifeq ($(OS),Windows_NT)
    TARGET = test_shell.exe
else
    TARGET = test_shell
endif

SRCS    = test_shell.c test_shell_mock.c unity/src/unity.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all build run clean unity

all: build run

build: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

# 下载 Unity 测试框架
unity:
	@if [ ! -d "unity" ]; then \
		echo "Downloading Unity..."; \
		git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git unity; \
	else \
		echo "Unity already exists."; \
	fi
