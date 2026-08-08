#
# ServEcosys 移动设备适配模块集 - 构建文件
#
# 以内核源码树 in-tree 方式编译（obj-m，随 make modules 产出 .ko）
# 产物: servecsys_mobile.ko
# 入口: obj-$(CONFIG_SERVECOSYS_MOBILE_MODULES) 由 CONFIG_SERVECOSYS_MOBILE_MODULES=m 决定

obj-$(CONFIG_SERVECOSYS_MOBILE_MODULES) += servecsys_mobile.o
servecsys_mobile-objs := mobile_modules.o

ccflags-y := -Wall