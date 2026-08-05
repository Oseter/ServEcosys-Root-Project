#
# ServEcosys 移动设备适配模块集 - 构建文件
#
# 以外部 kbuild 方式编译：
#   make -C <linux-src> M=$(pwd) modules
# 产物: servecosys_mobile.ko
#
# 也可内联进内核树编译（CONFIG_SERVECOSYS_MOBILE_MODULES=m）

obj-m += servecosys_mobile.o
servecsys_mobile-objs := mobile_modules.o

ccflags-y := -Wall