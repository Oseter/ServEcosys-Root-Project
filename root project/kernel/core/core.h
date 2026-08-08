/**
 * ServEcosys Kernel Core - 内核中央公共头文件
 *
 * 内核中央只包含内核最基础的功能，每个基础功能是一个"原子"，
 * 各原子以 servecosys_<atom>_init() 形式暴露初始化入口，
 * 由 main.c（入口）统一编排。
 *
 * 原子清单（最基础功能）：
 *   - sched.c       进程调度
 *   - mm.c          内存管理
 *   - net.c         网络协议栈
 */

#ifndef _SERVECOSYS_CORE_H_
#define _SERVECOSYS_CORE_H_

#include <linux/types.h>

#define SERVECOSYS_VERSION   "0.1.0"
#define SERVECOSYS_CODENAME  "Genesis"

/* 各原子的初始化入口（由 main.c 在 early_initcall 中编排） */
int servecosys_sched_init(void);
int servecosys_mm_init(void);
int servecosys_net_init(void);

#endif /* _SERVECOSYS_CORE_H_ */
