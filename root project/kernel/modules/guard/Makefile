# ServEcosys Guard - 0day detection & mitigation kernel module
#
# 以外部 kbuild 方式编译：
#   make -C <linux-src> M=$(pwd) modules
# 产物: servecosys_guard.ko
#
# 也可内联进内核树编译（CONFIG_SERVECOSYS_GUARD=m 见 servecosys_defconfig）

obj-m += servecosys_guard.o
servecosys_guard-objs := guard.o

ccflags-y := -Wall -Wextra

# 若 probe 模块先于 guard 构建，可在此补充其符号路径（当前无需）
