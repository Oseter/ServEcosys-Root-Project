#
# ServEcosys Frontend Dependency Libraries (.ssle)
#
# 每个依赖独立文件夹，.ssle 应用按需调用
#

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE -fPIC
OUTPUT_DIR = ../../build/frontend-deps

DEPS = \
	log \
	ipc \
	display \
	input \
	app

.PHONY: all clean $(DEPS)

all: $(OUTPUT_DIR) $(DEPS)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

define BUILD_DEP
$(1): $(OUTPUT_DIR)
	@echo "[DEPS:ssle] Building $(1)..."
	$(CC) $(CFLAGS) -c deps/$(1)/$(1).c -o $(OUTPUT_DIR)/$(1).o
	$(AR) rcs $(OUTPUT_DIR)/lib$(1).a $(OUTPUT_DIR)/$(1).o
	@echo "[DEPS:ssle] lib$(1).a built"
endef

$(foreach dep,$(DEPS),$(eval $(call BUILD_DEP,$(dep))))

clean:
	@echo "[DEPS:ssle] Cleaning..."
	rm -rf $(OUTPUT_DIR)

list:
	@echo "Frontend dependency libraries (for .ssle):"
	@for dep in $(DEPS); do \
		echo "  $$dep/    -> lib$$dep.a"; \
	done
