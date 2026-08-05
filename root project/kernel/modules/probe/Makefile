#
# ServEcosys 硬件指纹探测模块 - 构建文件
#
# 以外部 kbuild 方式编译：
#   make -C <linux-src> M=$(pwd) modules
# 产物: servecosys_probe.ko
#
# 也可内联进内核树编译（CONFIG_SERVECOSYS_PROBE=m）

obj-m += servecosys_probe.o
servecsys_probe-objs := probe_modules.o

ccflags-y := -Wall