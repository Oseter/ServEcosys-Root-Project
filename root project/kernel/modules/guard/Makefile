# ServEcosys Guard - 0day detection & mitigation kernel module
#
# 注意：guard 依赖 security_add_hooks（6.6 为 __init 且未导出），
#       只能内建进内核（CONFIG_SERVECOSYS_GUARD=y），不能作为
#       loadable 模块单独加载。
#
# 内建：CONFIG_SERVECOSYS_GUARD=y → obj-y（链接进内核映像）
# 外建：make -C <linux-src> M=$(pwd) modules（需内核已预编译）

obj-$(CONFIG_SERVECOSYS_GUARD) += servecsys_guard.o
servecsys_guard-objs := guard.o

ccflags-y := -Wall -Wextra

# 若 probe 模块先于 guard 构建，可在此补充其符号路径（当前无需）
