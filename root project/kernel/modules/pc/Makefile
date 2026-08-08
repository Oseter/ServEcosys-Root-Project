#
# ServEcosys PC 设备适配模块集 - 构建文件
#
# 以内核源码树 in-tree 方式编译（obj-m，随 make modules 产出 .ko）
# 产物: servecsys_pc.ko
# 入口: obj-$(CONFIG_SERVECOSYS_PC_MODULES) 由 CONFIG_SERVECOSYS_PC_MODULES=m 决定

obj-$(CONFIG_SERVECOSYS_PC_MODULES) += servecsys_pc.o
servecsys_pc-objs := pc_modules.o

ccflags-y := -Wall