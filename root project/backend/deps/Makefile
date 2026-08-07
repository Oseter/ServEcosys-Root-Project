#
# ServEcosys Backend Dependency Libraries (.smle)
#
# 每个依赖独立文件夹，.smle 服务按需调用
#

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE -fPIC
# 可由主 Makefile 以 OUTPUT_DIR=... 覆盖（命令行优先级高于 ?=）
OUTPUT_DIR ?= ../../build/backend-deps

DEPS = \
	crypto \
	log \
	ipc \
	perm \
	selinux \
	hal \
	btrfs \
	oipes-net

# 源文件名覆盖：dep 名与实际 .c 文件名不一致时用 _SRC 指定（默认 <dep>/<dep>.c）
selinux_SRC   = selinux/selinux_wrap.c
btrfs_SRC     = btrfs/btrfs_utils.c
oipes-net_SRC = oipes-net/oipes.c

.PHONY: all clean $(DEPS)

all: $(OUTPUT_DIR) $(DEPS)

$(OUTPUT_DIR):
	mkdir -p "$(OUTPUT_DIR)"

define BUILD_DEP
$(1): $(OUTPUT_DIR)
	@echo "[DEPS:smle] Building $(1)..."
	$(CC) $(CFLAGS) -c "$(or $($(1)_SRC),$(1)/$(1).c)" -o "$(OUTPUT_DIR)/$(1).o"
	$(AR) rcs "$(OUTPUT_DIR)/lib$(1).a" "$(OUTPUT_DIR)/$(1).o"
	@echo "[DEPS:smle] lib$(1).a built"
endef

$(foreach dep,$(DEPS),$(eval $(call BUILD_DEP,$(dep))))

clean:
	@echo "[DEPS:smle] Cleaning..."
	rm -rf $(OUTPUT_DIR)

list:
	@echo "Backend dependency libraries (for .smle):"
	@for dep in $(DEPS); do \
		echo "  $$dep/    -> lib$$dep.a"; \
	done