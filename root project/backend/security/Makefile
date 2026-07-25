#
# ServEcosys SELinux Policy Makefile
#
# Build SELinux policy modules for ServEcosys
#

POLICY_NAME = servecosys
POLICY_DIR = $(shell pwd)
OUTPUT_DIR = $(POLICY_DIR)/../../build/selinux

# Toolchain
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

all: $(OUTPUT_DIR)/$(PP_BIN)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(OUTPUT_DIR)/$(POLICY_NAME).mod: $(TE_SRC) $(IF_SRC) | $(OUTPUT_DIR)
	@echo "[CHECKMODULE] Building policy module..."
	$(CHECKMODULE) $(CHECKMODULE_FLAGS) -o $@ $(TE_SRC)

$(OUTPUT_DIR)/$(PP_BIN): $(OUTPUT_DIR)/$(POLICY_NAME).mod $(FC_SRC)
	@echo "[SEMODULE_PKG] Packaging policy..."
	$(SEMODULE_PKG) -o $@ -m $(OUTPUT_DIR)/$(POLICY_NAME).mod -f $(FC_SRC)
	@echo "[OK] SELinux policy module built: $@"

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
