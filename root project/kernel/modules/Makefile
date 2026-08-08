#
# ServEcosys 内核模块 - 递归构建入口
#
# 由 scripts/apply_kernel_patch.sh 同步到内核源码树
# kernel/servecosys/modules/Makefile，并挂接进内核构建递归链
# （kernel/servecosys/Makefile 以 obj-y += modules/ 递归进入）。
#
# 各模块按 CONFIG_* 选择：
#   - guard  (CONFIG_SERVECOSYS_GUARD=y)    内建进内核映像
#   - probe  (CONFIG_SERVECOSYS_PROBE=m)    独立 .ko
#   - pc     (CONFIG_SERVECOSYS_PC_MODULES=m) 独立 .ko
#   - mobile (CONFIG_SERVECOSYS_MOBILE_MODULES=m) 独立 .ko
#

obj-$(CONFIG_SERVECOSYS_GUARD)             += guard/
obj-$(CONFIG_SERVECOSYS_PROBE)             += probe/
obj-$(CONFIG_SERVECOSYS_PC_MODULES)        += pc/
obj-$(CONFIG_SERVECOSYS_MOBILE_MODULES)    += mobile/