#
# ServEcosys SED Backend Daemons (.smle)
#
# Build all SED services with backend dependency libraries
#

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE
OUTPUT_DIR ?= $(abspath ../../build/sed)
DEPS_DIR ?= ../deps
DEPS_BUILD ?= $(abspath ../../build/backend-deps)
INCLUDES = -I$(DEPS_DIR)/crypto -I$(DEPS_DIR)/log -I$(DEPS_DIR)/ipc \
           -I$(DEPS_DIR)/perm -I$(DEPS_DIR)/selinux -I$(DEPS_DIR)/hal \
           -I$(DEPS_DIR)/btrfs -I$(DEPS_DIR)/oipes-net

# make 在 target/prerequisite 中按空白拆分路径，含空格的 abspath（如
# ".../root project/build/..."）必须用 rpt space 转义才能作为单 target；
# recipe 内用引号包住原始值即可。
empty :=
space := $(empty) $(empty)
OUTPUT_DIR_ESC := $(subst $(space),\ ,$(OUTPUT_DIR))
DEPS_BUILD_ESC := $(subst $(space),\ ,$(DEPS_BUILD))

# .smle 服务依赖声明（主 Makefile 已通过 sed-deps 目标构建依赖库；此处直接引用）
# prerequisite 用 DEPS_BUILD_ESC（转义空格），recipe 内用原始 $(DEPS_BUILD)（引号保护）
selinux_manager_libs    = $(DEPS_BUILD_ESC)/liblog.a $(DEPS_BUILD_ESC)/libselinux.a
hal_manager_libs        = $(DEPS_BUILD_ESC)/liblog.a $(DEPS_BUILD_ESC)/libhal.a
permission_arbiter_libs = $(DEPS_BUILD_ESC)/liblog.a $(DEPS_BUILD_ESC)/libipc.a $(DEPS_BUILD_ESC)/libperm.a $(DEPS_BUILD_ESC)/libselinux.a
oipes_client_libs       = $(DEPS_BUILD_ESC)/liblog.a $(DEPS_BUILD_ESC)/liboipes-net.a $(DEPS_BUILD_ESC)/libcrypto.a
ipc_bus_libs            = $(DEPS_BUILD_ESC)/liblog.a $(DEPS_BUILD_ESC)/libipc.a
system_manager_libs     = $(DEPS_BUILD_ESC)/liblog.a $(DEPS_BUILD_ESC)/libipc.a

SRCS = selinux_manager.c hal_manager.c permission_arbiter.c oipes_client.c ipc_bus.c system_manager.c
TARGETS = $(SRCS:.c=)

all: deps $(OUTPUT_DIR_ESC) $(TARGETS)

deps:
	@$(MAKE) -C $(DEPS_DIR) all OUTPUT_DIR="$(DEPS_BUILD)" DEPS_DIR="$(DEPS_DIR)"

$(OUTPUT_DIR_ESC):
	mkdir -p "$(OUTPUT_DIR)"

selinux_manager: selinux_manager.c $(selinux_manager_libs)
	@echo "[SED:smle] Building selinux_manager (deps: log selinux)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o "$(OUTPUT_DIR)/$@.smle" $< $(selinux_manager_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

hal_manager: hal_manager.c $(hal_manager_libs)
	@echo "[SED:smle] Building hal_manager (deps: log hal)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o "$(OUTPUT_DIR)/$@.smle" $< $(hal_manager_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

permission_arbiter: permission_arbiter.c $(permission_arbiter_libs)
	@echo "[SED:smle] Building permission_arbiter (deps: log ipc perm selinux)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o "$(OUTPUT_DIR)/$@.smle" $< $(permission_arbiter_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

oipes_client: oipes_client.c $(oipes_client_libs)
	@echo "[SED:smle] Building oipes_client (deps: log oipes-net crypto)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o "$(OUTPUT_DIR)/$@.smle" $< $(oipes_client_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

ipc_bus: ipc_bus.c $(ipc_bus_libs)
	@echo "[SED:smle] Building ipc_bus (deps: log ipc)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o "$(OUTPUT_DIR)/$@.smle" $< $(ipc_bus_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

system_manager: system_manager.c $(system_manager_libs)
	@echo "[SED:smle] Building system_manager (deps: log ipc)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o "$(OUTPUT_DIR)/$@.smle" $< $(system_manager_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

clean:
	@echo "[SED:smle] Cleaning..."
	rm -rf $(OUTPUT_DIR)

distclean: clean
	@$(MAKE) -C $(DEPS_DIR) clean

install: all
	@if [ -d /system/backend/bin ]; then \
		cp $(OUTPUT_DIR)/*.smle /system/backend/bin/; \
		echo "[OK] Installed to /system/backend/bin/"; \
	fi

info:
	@echo "ServEcosys SED Backend Services (.smle)"
	@while IFS=: read -r service deps; do \
		echo "  $$service.smle  -> $$deps"; \
	done < deps.lst

.PHONY: all clean distclean install deps info
