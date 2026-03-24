/**
 * 
 * @file shell.h
 * @author liu (lbq08@foxmail.com)
 * @brief shell模块头文件 - 精简CLI版本
 * 
 * @copyright Copyright (c) 2026 liu 
 * For study and research only
 */
#ifndef __SHELL_H
#define __SHELL_H

#include "shell_cfg.h"

#include <stdint.h>

/* 命令结构体 */
typedef struct
{
    const char* name;
    const char* desc;
    int (*func)(int argc, char* argv[]);
#if SHELL_USING_AUTH
    uint8_t permission;    /* 权限掩码: 0=无限制, 其他=需要对应权限 */
#endif
#if SHELL_USING_COMPLETION
    const char** comp_list; /* 参数补全列表 (NULL结尾), 可为NULL */
#endif
} shell_cmd_t;

/* ==================== 变量导出 ==================== */
#if SHELL_USING_VAR

/* 变量类型 */
typedef enum {
    SHELL_VAR_INT,      /* int 型 */
    SHELL_VAR_UINT,     /* unsigned int 型 */
    SHELL_VAR_FLOAT,    /* float 型 */
    SHELL_VAR_BOOL,     /* bool 型 (0/1) */
    SHELL_VAR_STRING,   /* 字符串 (只读) */
} shell_var_type_t;

/* 变量结构体 */
typedef struct {
    const char*      name;      /* 变量名 */
    void*            ptr;       /* 变量指针 */
    shell_var_type_t type;      /* 变量类型 */
    uint8_t          readonly;  /* 只读标志 */
} shell_var_t;

#endif /* SHELL_USING_VAR */

#if SHELL_USING_AUTH
/* 用户结构体 */
typedef struct
{
    const char* name;       /* 用户名 */
    const char* password;   /* 密码 (空字符串=无密码) */
    uint8_t     permission; /* 权限掩码 */
} shell_user_t;

/* 权限等级定义 */
#define SHELL_PERM_NONE   0x00  /* 无限制 */
#define SHELL_PERM_USER   0x01  /* 普通用户 */
#define SHELL_PERM_ADMIN  0x02  /* 管理员 */
#define SHELL_PERM_ROOT   0xFF  /* 超级用户 */
#endif

/* ==================== 命令导出宏 ==================== */
/*
 * 使用方法：在任意 .c 文件中添加命令：
 *   SHELL_EXPORT_CMD(test, "Test command", cmd_test);
 *
 * 链接脚本需要添加：
 *   .shell_cmd :
 *   {
 *       . = ALIGN(4);
 *       __shell_cmd_start = .;
 *       KEEP(*(.shell_cmd*))
 *       __shell_cmd_end = .;
 *   } > FLASH
 */

/* Keil/ARMCC 编译器 */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
    #define SHELL_USED    __attribute__((used))
    #define SHELL_SECTION __attribute__((section("SHELL_CMD")))
/* GCC 编译器 */
#elif defined(__GNUC__)
    #define SHELL_USED    __attribute__((used))
    #define SHELL_SECTION __attribute__((section(".shell_cmd")))
/* IAR 编译器 */
#elif defined(__ICCARM__)
    #define SHELL_USED
    #define SHELL_SECTION @ ".shell_cmd"
#else
    #define SHELL_USED
    #define SHELL_SECTION
#endif

/**
 * @brief 命令导出宏
 * @param _name 命令名称（不加引号）
 * @param _desc 命令描述（字符串）
 * @param _func 命令函数（int func(int argc, char* argv[])）
 * @param _perm 权限掩码（仅 SHELL_USING_AUTH 启用时有效）
 */
#if SHELL_USING_AUTH && SHELL_USING_COMPLETION
/* 带权限和补全列表 */
#define SHELL_EXPORT_CMD(_name, _desc, _func, _perm) \
    SHELL_USED const shell_cmd_t __shell_cmd_##_name SHELL_SECTION = { \
        .name = #_name, .desc = _desc, .func = _func, \
        .permission = _perm, .comp_list = NULL, \
    }
#define SHELL_EXPORT_CMD_LIST(_name, _desc, _func, _perm, _list) \
    SHELL_USED const shell_cmd_t __shell_cmd_##_name SHELL_SECTION = { \
        .name = #_name, .desc = _desc, .func = _func, \
        .permission = _perm, .comp_list = _list, \
    }
#elif SHELL_USING_AUTH
#define SHELL_EXPORT_CMD(_name, _desc, _func, _perm) \
    SHELL_USED const shell_cmd_t __shell_cmd_##_name SHELL_SECTION = { \
        .name = #_name, .desc = _desc, .func = _func, .permission = _perm, \
    }
#elif SHELL_USING_COMPLETION
#define SHELL_EXPORT_CMD(_name, _desc, _func, ...) \
    SHELL_USED const shell_cmd_t __shell_cmd_##_name SHELL_SECTION = { \
        .name = #_name, .desc = _desc, .func = _func, .comp_list = NULL, \
    }
#define SHELL_EXPORT_CMD_LIST(_name, _desc, _func, _perm, _list) \
    SHELL_USED const shell_cmd_t __shell_cmd_##_name SHELL_SECTION = { \
        .name = #_name, .desc = _desc, .func = _func, .comp_list = _list, \
    }
#else
#define SHELL_EXPORT_CMD(_name, _desc, _func, ...) \
    SHELL_USED const shell_cmd_t __shell_cmd_##_name SHELL_SECTION = { \
        .name = #_name, .desc = _desc, .func = _func, \
    }
#endif

/* ==================== 变量导出宏 ==================== */
#if SHELL_USING_VAR

/* Keil/ARMCC 编译器 */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
    #define SHELL_VAR_SECTION __attribute__((section("SHELL_VAR")))
#elif defined(__GNUC__)
    #define SHELL_VAR_SECTION __attribute__((section(".shell_var")))
#elif defined(__ICCARM__)
    #define SHELL_VAR_SECTION @ ".shell_var"
#else
    #define SHELL_VAR_SECTION
#endif

/**
 * @brief 导出变量宏
 * @param _name 变量名（不加引号）
 * @param _ptr  变量指针
 * @param _type 变量类型 (SHELL_VAR_INT/UINT/FLOAT/BOOL/STRING)
 */
#define SHELL_EXPORT_VAR(_name, _ptr, _type) \
    SHELL_USED const shell_var_t __shell_var_##_name SHELL_VAR_SECTION = { \
        .name = #_name, \
        .ptr = (void*)(_ptr), \
        .type = _type, \
        .readonly = 0, \
    }

/**
 * @brief 导出只读变量宏
 */
#define SHELL_EXPORT_VAR_RO(_name, _ptr, _type) \
    SHELL_USED const shell_var_t __shell_var_##_name SHELL_VAR_SECTION = { \
        .name = #_name, \
        .ptr = (void*)(_ptr), \
        .type = _type, \
        .readonly = 1, \
    }

/* 变量段符号 */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
    extern const shell_var_t Image$$SHELL_VAR$$Base[];
    extern const shell_var_t Image$$SHELL_VAR$$Limit[];
    #define __shell_var_start  Image$$SHELL_VAR$$Base
    #define __shell_var_end    Image$$SHELL_VAR$$Limit
#elif defined(__ICCARM__)
    #pragma section = ".shell_var"
    #define __shell_var_start  ((const shell_var_t *)__section_begin(".shell_var"))
    #define __shell_var_end    ((const shell_var_t *)__section_end(".shell_var"))
#else
    extern const shell_var_t __shell_var_start[];
    extern const shell_var_t __shell_var_end[];
#endif

#define SHELL_VAR_LIST()   (__shell_var_start)
#define SHELL_VAR_COUNT()  ((uint16_t)(__shell_var_end - __shell_var_start))

#endif /* SHELL_USING_VAR */

/* 
 * 宏导出功能说明：
 * Keil scatter 文件需要添加 SHELL_CMD 区域
 * 使用 SHELL_EXPORT_CMD 宏在任意 .c 文件中注册命令
 */
#if SHELL_USING_CMD_EXPORT
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
    /* Keil/ARMCC: 使用链接器区域符号 (SHELL_CMD 是 scatter 文件中的执行区域名) */
    extern const shell_cmd_t Image$$SHELL_CMD$$Base[];
    extern const shell_cmd_t Image$$SHELL_CMD$$Limit[];
    #define __shell_cmd_start  Image$$SHELL_CMD$$Base
    #define __shell_cmd_end    Image$$SHELL_CMD$$Limit
#elif defined(__ICCARM__)
    /* IAR: 使用段属性 */
    #pragma section = ".shell_cmd"
    #define __shell_cmd_start  ((const shell_cmd_t *)__section_begin(".shell_cmd"))
    #define __shell_cmd_end    ((const shell_cmd_t *)__section_end(".shell_cmd"))
#else
    /* GCC: 使用标准段符号 */
    extern const shell_cmd_t __shell_cmd_start[];
    extern const shell_cmd_t __shell_cmd_end[];
#endif

/* 获取导出命令列表 */
#define SHELL_CMD_LIST()   (__shell_cmd_start)
#define SHELL_CMD_COUNT()  ((uint16_t)(__shell_cmd_end - __shell_cmd_start))
#endif  /* SHELL_USING_CMD_EXPORT */

/* Shell控制块 */
typedef struct
{
    char     cmd_buf[SHELL_CMD_SIZE];
    uint16_t cmd_len;
    uint16_t cmd_pos;

#if SHELL_USING_HISTORY
    char    hist[SHELL_HISTORY_MAX][SHELL_CMD_SIZE];
    uint8_t hist_cnt;
    uint8_t hist_idx;
    uint8_t hist_cur;
#endif

#if SHELL_RX_BUF_SIZE > 0
    /* 内置环形接收缓冲区 */
    volatile uint8_t  rx_buf[SHELL_RX_BUF_SIZE];
    volatile uint16_t rx_head;  /* 写入位置 (中断写) */
    volatile uint16_t rx_tail;  /* 读取位置 (主循环读) */
#endif

    const shell_cmd_t* cmds;
    uint16_t           cmd_cnt;

    uint8_t esc_state;
    uint8_t esc_buf[4];
    uint8_t esc_idx;
    uint8_t is_active;  /* 命令执行中标志，用于尾行模式优化 */
    uint8_t is_inited;  /* 首次任务初始化标志 */

#if SHELL_USING_PASSTHROUGH
    uint8_t passthrough;
    void (*pt_handler)(uint8_t ch);
#endif

#if SHELL_USING_AUTH
    const shell_user_t* users;      /* 用户列表 */
    uint8_t             user_cnt;   /* 用户数量 */
    const shell_user_t* cur_user;   /* 当前用户 */
    uint8_t             is_checked; /* 密码已校验 */
#endif

    void (*write)(const char* data, uint16_t len);
    int (*read)(char* data, uint16_t len);
} shell_t;

/* 全局shell指针 */
extern shell_t* g_shell;

/* API */
void shell_init(shell_t* sh, const shell_cmd_t* cmds, uint16_t cnt, void (*write)(const char*, uint16_t),
                int (*read)(char*, uint16_t));
void shell_task(shell_t* sh);
void shell_input(shell_t* sh, char ch);
void shell_print(shell_t* sh, const char* str);
void shell_printf(shell_t* sh, const char* fmt, ...);
void shell_log(const char* buf, int len);

/* 环形缓冲区API (中断安全) */
#if SHELL_RX_BUF_SIZE > 0
void shell_rx_push(shell_t* sh, uint8_t ch);       /* 中断中调用: 写入单字节 */
void shell_rx_push_buf(shell_t* sh, const uint8_t* data, uint16_t len); /* 中断中调用: 写入多字节 */
int  shell_rx_read(shell_t* sh, char* buf, uint16_t max_len); /* 主循环调用: 读取数据 */
#endif

#if SHELL_USING_PASSTHROUGH
void shell_set_passthrough(shell_t* sh, void (*handler)(uint8_t));
void shell_exit_passthrough(shell_t* sh);
#endif

/* 内置命令 */
int cmd_help(int argc, char* argv[]);
int cmd_clear(int argc, char* argv[]);
#if SHELL_USING_HISTORY
int cmd_history(int argc, char* argv[]);
#endif
#if SHELL_USING_VAR
int cmd_var(int argc, char* argv[]);
int cmd_vars(int argc, char* argv[]);
#endif

#if SHELL_USING_AUTH
/* 用户认证 API */
void shell_set_users(shell_t* sh, const shell_user_t* users, uint8_t cnt);
int  shell_login(shell_t* sh, const char* name, const char* password);
void shell_logout(shell_t* sh);
int  cmd_login(int argc, char* argv[]);
int  cmd_logout(int argc, char* argv[]);
int  cmd_whoami(int argc, char* argv[]);
#endif

/* 使用导出命令初始化 (配合 SHELL_EXPORT_CMD 宏，需链接脚本支持) */
#if SHELL_USING_CMD_EXPORT
#define shell_init_export(sh, write, read) \
    shell_init(sh, SHELL_CMD_LIST(), SHELL_CMD_COUNT(), write, read)
#endif

#endif
