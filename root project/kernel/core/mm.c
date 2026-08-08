/**
 * ServEcosys Kernel Core - 原子：内存管理
 *
 * 内存管理由内核原生提供（分页/虚拟内存/THP/KSM）。本原子作为内核
 * 中央对该基础功能的统一登记点，纳入初始化序列。
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

int __init servecosys_mm_init(void)
{
    pr_info("ServEcosys: 原子[mm] 内存管理基础就绪 (分页/THP/KSM)\n");
    return 0;
}