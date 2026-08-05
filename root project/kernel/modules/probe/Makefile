#
# ServEcosys 硬件指纹探测模块 - 构建文件
#
# 以内核源码树 in-tree 方式编译（obj-m，随 make modules 产出 .ko）
# 产物: servecsys_probe.ko
# 入口: obj-$(CONFIG_SERVECOSYS_PROBE) 由 CONFIG_SERVECOSYS_PROBE=m 决定
# 注意: obj 名 servecsys_probe 必须与 servecsys_probe-objs 完全一致，
#       否则 kbuild 找不到源文件（No rule to make target）。

obj-$(CONFIG_SERVECOSYS_PROBE) += servecsys_probe.o
servecsys_probe-objs := probe_modules.o

ccflags-y := -Wall