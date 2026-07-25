#
# ServEcosys UID Frontend Daemons (.ssle)
#
# Build all UID services with frontend dependency libraries
#

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE
OUTPUT_DIR = ../../build/uid
DEPS_DIR = ../deps
DEPS_BUILD = ../../build/frontend-deps
INCLUDES = -I$(DEPS_DIR)/log -I$(DEPS_DIR)/ipc \
           -I$(DEPS_DIR)/display -I$(DEPS_DIR)/input

# .ssle 应用依赖声明
display_server_libs = $(DEPS_BUILD)/liblog.a $(DEPS_BUILD)/libdisplay.a $(DEPS_BUILD)/libipc.a
input_manager_libs  = $(DEPS_BUILD)/liblog.a $(DEPS_BUILD)/libinput.a $(DEPS_BUILD)/libipc.a
compositor_libs     = $(DEPS_BUILD)/liblog.a $(DEPS_BUILD)/libipc.a
system_ui_libs      = $(DEPS_BUILD)/liblog.a

SRCS = display_server.c input_manager.c compositor.c system_ui.c
TARGETS = $(SRCS:.c=)

all: deps $(OUTPUT_DIR) $(TARGETS)

deps:
	@$(MAKE) -C $(DEPS_DIR) all

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

display_server: display_server.c $(display_server_libs)
	@echo "[UID:ssle] Building display_server (deps: log display ipc)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT_DIR)/$@.ssle $< $(display_server_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.ssle"

input_manager: input_manager.c $(input_manager_libs)
	@echo "[UID:ssle] Building input_manager (deps: log input ipc)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT_DIR)/$@.ssle $< $(input_manager_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.ssle"

compositor: compositor.c $(compositor_libs)
	@echo "[UID:ssle] Building compositor (deps: log ipc)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT_DIR)/$@.ssle $< $(compositor_libs) -lpthread
	@echo "[OK] Built $(OUTPUT_DIR)/$@.ssle"

system_ui: system_ui.c $(system_ui_libs)
	@echo "[UID:ssle] Building system_ui (deps: log)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT_DIR)/$@.ssle $< $(system_ui_libs)
	@echo "[OK] Built $(OUTPUT_DIR)/$@.ssle"

clean:
	@echo "[UID:ssle] Cleaning..."
	rm -rf $(OUTPUT_DIR)

distclean: clean
	@$(MAKE) -C $(DEPS_DIR) clean

install: all
	@if [ -d /system/frontend/bin ]; then \
		cp $(OUTPUT_DIR)/*.ssle /system/frontend/bin/; \
		echo "[OK] Installed to /system/frontend/bin/"; \
	fi

info:
	@echo "ServEcosys UID Frontend Services (.ssle)"
	@while IFS=: read -r service deps; do \
		echo "  $$service.ssle  -> $$deps"; \
	done < deps.lst

.PHONY: all clean distclean install deps info
