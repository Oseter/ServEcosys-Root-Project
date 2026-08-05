/**
 * ServEcosys Kernel Core - 内核中央入口
 *
 * 内核中央只包含内核最基础的功能，每个基础功能是一个"原子"。
 * 本文件是内核中央的入口，负责在初始化序列中统一编排各原子。
 *
 * 原子清单（最基础功能）：
 *   - sched.c  进程调度
 *   - mm.c     内存管理
 *   - net.c    网络协议栈
 *
 * 设计原则：
 *   - 设备驱动与安全等能力由上层 / 可插拔模块承载，不入内核中央。
 *   - 内核中央保持最小化，仅承载内核最基础的功能。
 *
 * 根本理念：以用户体验需求为中心，为用户服务。
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include "core.h"

/* ============================================================================
 * 内核中央初始化序列：按最基础功能的依赖顺序编排各原子
 * ============================================================================ */

static int __init servecosys_early_init(void)
{
    int rc = 0;

    pr_info("ServEcosys 内核中央 v%s '%s' 启动\n",
            SERVECOSYS_VERSION, SERVECOSYS_CODENAME);

    rc |= servecosys_mm_init();     /* 内存管理（最早，其他原子依赖） */
    rc |= servecosys_sched_init();  /* 进程调度 */
    rc |= servecosys_net_init();    /* 网络协议栈 */

    if (rc)
        pr_err("ServEcosys: 内核中央部分原子初始化失败 (rc=0x%x)\n", rc);

    pr_info("ServEcosys 内核中央基础功能就绪. 设备驱动与安全能力由上层模块承载.\n");
    return 0;
}

early_initcall(servecosys_early_init);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ServEcosys Project");
MODULE_DESCRIPTION("ServEcosys Kernel Core - 内核中央（最基础功能入口）");
MODULE_VERSION(SERVECOSYS_VERSION);