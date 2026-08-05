/**
 * ServEcosys Kernel Core - 原子：网络协议栈
 *
 * 网络协议栈由内核原生提供（IPv4/IPv6 双栈、TCP/UDP、SCTP、路由、
 * netfilter）。本原子作为内核中央对该基础功能的统一登记点。
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

int __init servecosys_net_init(void)
{
    pr_info("ServEcosys: 原子[net] 网络协议栈基础就绪 (IPv4/IPv6, TCP/UDP)\n");
    return 0;
}