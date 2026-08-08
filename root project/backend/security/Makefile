#
# ServEcosys SELinux Policy Makefile
#
# Build SELinux policy modules for ServEcosys
#

POLICY_NAME = servecosys
POLICY_DIR = $(shell pwd)
OUTPUT_DIR ?= $(abspath $(POLICY_DIR)/../../build/selinux)

# make 的 target/prerequisite 会按空白拆分路径，含空格的 abspath
# （如 ".../root project/build/..."）需用 \ 转义空格作为单目标；
# recipe 内用引号包住原始 $(OUTPUT_DIR) 即可。
empty :=
space := $(empty) $(empty)
OUTPUT_DIR_ESC := $(subst $(space),\ ,$(OUTPUT_DIR))

# Toolchain
# 优先使用 refpolicy 的 devel Makefile（处理 m4 接口宏 + checkmodule + semodule_package）
# 提供方: apt install selinux-policy-dev (Ubuntu noble/universe)
REFPOL_MAKEFILE ?= /usr/share/selinux/devel/Makefile
CHECKPOLICY ?= checkpolicy
CHECKMODULE ?= checkmodule
SEMODULE_PKG ?= semodule_package
M4 ?= m4

# Flags
M4FLAGS = -D enable_debug -U enable_mls
CHECKPOLICY_FLAGS = -O -M -U deny
CHECKMODULE_FLAGS = -M -m

# Source files
TE_SRC = $(POLICY_NAME).te
FC_SRC = $(POLICY_NAME).fc
IF_SRC = $(POLICY_NAME).if
PP_BIN = $(POLICY_NAME).pp

all: $(OUTPUT_DIR_ESC)/$(PP_BIN)

$(OUTPUT_DIR_ESC):
	mkdir -p "$(OUTPUT_DIR)"

# refpolicy 方式：make -f devel/Makefile servecosys.pp（输出到当前目录，再搬走）
$(OUTPUT_DIR_ESC)/$(PP_BIN): $(TE_SRC) $(IF_SRC) $(FC_SRC) | $(OUTPUT_DIR_ESC)
	@echo "[POLICY] Building servecosys policy module (refpolicy makefile)..."
	$(MAKE) -f "$(REFPOL_MAKEFILE)" $(PP_BIN)
	cp -f "$(PP_BIN)" "$(OUTPUT_DIR)/$(PP_BIN)"
	@echo "[OK] SELinux policy module built: $(OUTPUT_DIR)/$(PP_BIN)"

clean:
	@echo "[CLEAN] Removing SELinux policy artifacts..."
	rm -rf $(OUTPUT_DIR)

install: $(OUTPUT_DIR)/$(PP_BIN)
	@echo "[INSTALL] Installing SELinux policy..."
	@if [ -d /etc/selinux/targeted/modules/active/modules ]; then \
		cp $(OUTPUT_DIR)/$(PP_BIN) /etc/selinux/targeted/modules/active/modules/; \
		echo "[OK] Policy installed. Run 'load_policy' to activate."; \
	else \
		echo "SELinux policy directory not found. Manual install required."; \
	fi

load: $(OUTPUT_DIR)/$(PP_BIN)
	@echo "[LOAD] Loading policy module..."
	@if command -v semodule > /dev/null 2>&1; then \
		semodule -i $(OUTPUT_DIR)/$(PP_BIN); \
		echo "[OK] Policy module loaded."; \
	else \
		echo "semodule not found. Install selinux-policy-devel tools."; \
	fi

.PHONY: all clean install load
