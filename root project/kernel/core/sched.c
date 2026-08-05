/**
 * ServEcosys Kernel Core - 原子：进程调度
 *
 * 进程调度由内核原生提供（CFS + RT + SMP）。本原子仅作为内核中央
 * 对该基础功能的统一登记点，确保其在内核中央的初始化序列中被确认，
 * 并为上层模块提供可扩展的调度集成接口（semantic 保留）。
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

int __init servecosys_sched_init(void)
{
    pr_info("ServEcosys: 原子[sched] 进程调度基础就绪 (CFS + RT)\n");
    return 0;
}