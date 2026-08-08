#
# ServEcosys Kernel Core - 内核中央 Makefile
#
# 内核中央编译为内建（obj-y），编译进内核映像本体。
# 由顶层内核 Makefile 通过子目录递归调用（需补丁将本目录加入
# 内核源码树的 Kconfig/Makefile 递归链）。
#

obj-y += servecsys_core.o
servecsys_core-y := main.o sched.o mm.o net.o

# 递归进入 modules/（其内部由 kernel/servecosys/modules/Makefile 按 CONFIG 选择）
obj-y += modules/

ccflags-y := -Wall