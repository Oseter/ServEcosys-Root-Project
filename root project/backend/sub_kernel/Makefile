#
# ServEcosys SED Backend Daemons (.smle)
#
# Build all SED services with backend dependency libraries
#

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE
OUTPUT_DIR = ../../build/sed
DEPS_DIR = ../deps
DEPS_BUILD = ../../build/backend-deps
INCLUDES = -I$(DEPS_DIR)/crypto -I$(DEPS_DIR)/log -I$(DEPS_DIR)/ipc \
           -I$(DEPS_DIR)/perm -I$(DEPS_DIR)/selinux -I$(DEPS_DIR)/hal \
           -I$(DEPS_DIR)/btrfs -I$(DEPS_DIR)/oipes-net

# .smle 服务依赖声明
selinux_manager_libs    = $(DEPS_BUILD)/liblog.a $(DEPS_BUILD)/libselinux.a
hal_manager_libs        = $(DEPS_BUILD)/liblog.a $(DEPS_BUILD)/libhal.a
permission_arbiter_libs = $(DEPS_BUILD)/liblog.a $(DEPS_BUILD)/libipc.a $(DEPS_BUILD)/libperm.a $(DEPS_BUILD)/libselinux.a
oipes_client_libs       = $(DEPS_BUILD)/liblog.a $(DEPS_BUILD)/liboipes-net.a $(DEPS_BUILD)/libcrypto.a
ipc_bus_libs            = $(DEPS_BUILD)/liblog.a $(DEPS_BUILD)/libipc.a
system_manager_libs     = $(DEPS_BUILD)/liblog.a $(DEPS_BUILD)/libipc.a

SRCS = selinux_manager.c hal_manager.c permission_arbiter.c oipes_client.c ipc_bus.c system_manager.c
TARGETS = $(SRCS:.c=)

all: deps $(OUTPUT_DIR) $(TARGETS)

deps:
	@$(MAKE) -C $(DEPS_DIR) all

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

selinux_manager: selinux_manager.c $(selinux_manager_libs)
	@echo "[SED:smle] Building selinux_manager (deps: log selinux)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT_DIR)/$@.smle $< $(selinux_manager_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

hal_manager: hal_manager.c $(hal_manager_libs)
	@echo "[SED:smle] Building hal_manager (deps: log hal)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT_DIR)/$@.smle $< $(hal_manager_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

permission_arbiter: permission_arbiter.c $(permission_arbiter_libs)
	@echo "[SED:smle] Building permission_arbiter (deps: log ipc perm selinux)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT_DIR)/$@.smle $< $(permission_arbiter_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

oipes_client: oipes_client.c $(oipes_client_libs)
	@echo "[SED:smle] Building oipes_client (deps: log oipes-net crypto)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT_DIR)/$@.smle $< $(oipes_client_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

ipc_bus: ipc_bus.c $(ipc_bus_libs)
	@echo "[SED:smle] Building ipc_bus (deps: log ipc)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT_DIR)/$@.smle $< $(ipc_bus_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.smle"

system_manager: system_manager.c $(system_manager_libs)
	@echo "[SED:smle] Building system_manager (deps: log ipc)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT_DIR)/$@.smle $< $(system_manager_libs) -lpthread
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
