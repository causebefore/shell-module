/**
 * @file    shell_ansi.h
 * @brief   ANSI颜色和光标控制定义
 */

#ifndef SHELL_ANSI_H_
#define SHELL_ANSI_H_

/* ==================== 光标控制宏 ==================== */

#define ANSI_CLEAR   "\033[2J\033[H"  /* 清屏并移动光标到左上角 */
#define ANSI_CLEARLN "\033[2K\r"      /* 清除当前行 */
#define ANSI_HOME    "\033[H"         /* 光标移动到左上角 */
#define ANSI_UP      "\033[1A"        /* 光标上移1行 */
#define ANSI_DOWN    "\033[1B"        /* 光标下移1行 */
#define ANSI_RIGHT   "\033[1C"        /* 光标右移1列 */
#define ANSI_LEFT    "\033[1D"        /* 光标左移1列 */

/* ==================== 前景色宏 ==================== */

#define ANSI_COLOR_RESET   "\033[0m"    /* 重置所有属性 */
#define ANSI_COLOR_RED     "\033[31m"   /* 红色 */
#define ANSI_COLOR_GREEN   "\033[32m"   /* 绿色 */
#define ANSI_COLOR_YELLOW  "\033[33m"   /* 黄色 */
#define ANSI_COLOR_BLUE    "\033[34m"   /* 蓝色 */
#define ANSI_COLOR_MAGENTA "\033[35m"   /* 品红 */
#define ANSI_COLOR_CYAN    "\033[36m"   /* 青色 */
#define ANSI_COLOR_WHITE   "\033[37m"   /* 白色 */
#define ANSI_COLOR_DEFAULT "\033[39m"   /* 默认前景色 */

/* ==================== 背景色宏 ==================== */

#define ANSI_BG_RED     "\033[41m"   /* 红色背景 */
#define ANSI_BG_GREEN   "\033[42m"   /* 绿色背景 */
#define ANSI_BG_YELLOW  "\033[43m"   /* 黄色背景 */
#define ANSI_BG_BLUE    "\033[44m"   /* 蓝色背景 */
#define ANSI_BG_MAGENTA "\033[45m"   /* 品红背景 */
#define ANSI_BG_CYAN    "\033[46m"   /* 青色背景 */
#define ANSI_BG_WHITE   "\033[47m"   /* 白色背景 */
#define ANSI_BG_DEFAULT "\033[49m"   /* 默认背景色 */

/* ==================== 文本样式宏 ==================== */

#define ANSI_BOLD      "\033[1m"   /* 粗体 */
#define ANSI_UNDERLINE "\033[4m"   /* 下划线 */
#define ANSI_BLINK     "\033[5m"   /* 闪烁 */
#define ANSI_REVERSE   "\033[7m"   /* 反显 */

#endif /* SHELL_ANSI_H_ */
