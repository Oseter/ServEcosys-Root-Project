#
# ServEcosys 设备产品链接文件 - qemu-x86_64
#
# 供顶层 Makefile 通过 include 加载该设备的 BoardConfig。
# 用法: make DEVICE=qemu-x86_64 <target>
#

-include devices/$(DEVICE)/BoardConfig.mk