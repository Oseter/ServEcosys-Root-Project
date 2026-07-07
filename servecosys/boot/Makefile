#
# ServEcosys Bootloader Makefile
#
# 编译 UEFI 引导程序
#
# 用法:
#   make              # 编译 x86_64 版本
#   make ARCH=arm64   # 编译 ARM64 版本
#   make clean        # 清理构建产物
#   make install      # 安装到 ESP 分区
#

# 架构配置
ARCH ?= x86_64
TARGET = bootloader

# 工具链
ifeq ($(ARCH),x86_64)
    EFI_ARCH = x86_64
    EFI_CFLAGS = -DEFI_X86_64
    EFI_LDS = /usr/lib/efi/crt0-efi-x86_64.lds
    OBJCOPY_TARGET = efi-app-x86_64
else ifeq ($(ARCH),arm64)
    EFI_ARCH = aarch64
    EFI_CFLAGS = -DEFI_AARCH64
    EFI_LDS = /usr/lib/efi/crt0-efi-aarch64.lds
    OBJCOPY_TARGET = efi-app-aarch64
else ifeq ($(ARCH),ia32)
    EFI_ARCH = ia32
    EFI_CFLAGS = -DEFI_IA32
    EFI_LDS = /usr/lib/efi/crt0-efi-ia32.lds
    OBJCOPY_TARGET = efi-app-ia32
else
    $(error Unsupported ARCH: $(ARCH). Supported: x86_64, arm64, ia32)
endif

# 编译器标志
CC = gcc
LD = ld
OBJCOPY = objcopy

CFLAGS = -I/usr/include/efi \
         -I/usr/include/efi/$(EFI_ARCH) \
         -I/usr/include/efi/protocol \
         -fno-stack-protector \
         -fpic \
         -fshort-wchar \
         -mno-red-zone \
         -Wall \
         -Wextra \
         -Werror \
         $(EFI_CFLAGS) \
         -DEFI_FUNCTION_WRAPPER \
         -O2

LDFLAGS = -nostdlib \
          -znocombreloc \
          -T $(EFI_LDS) \
          -L/usr/lib \
          -lefi \
          -lgnuefi \
          -O2 \
          -Bsymbolic

OBJCOPY_FLAGS = -j .text \
                -j .sdata \
                -j .data \
                -j .dynamic \
                -j .dynsym \
                -j .rel \
                -j .rela \
                -j .reloc \
                --target=$(OBJCOPY_TARGET)

# 源文件
SRCS = bootloader.c
OBJS = $(SRCS:.c=.o)
TARGET_SO = $(TARGET).so
TARGET_EFI = $(TARGET).efi

# 默认目标
all: $(TARGET_EFI)

# 编译目标
$(TARGET_EFI): $(OBJS)
	@echo "[LD] Linking $(TARGET_SO)..."
	$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET_SO)
	
	@echo "[OBJCOPY] Generating $(TARGET_EFI)..."
	$(OBJCOPY) $(OBJCOPY_FLAGS) $(TARGET_SO) $(TARGET_EFI)
	
	@echo "[OK] Built $(TARGET_EFI) ($(ARCH))"

# 编译源文件
%.o: %.c
	@echo "[CC] Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	@echo "[CLEAN] Removing build artifacts..."
	rm -f $(OBJS) $(TARGET_SO) $(TARGET_EFI)
	@echo "[OK] Clean complete"

# 安装到 ESP 分区
install: $(TARGET_EFI)
	@echo "[INSTALL] Copying to ESP partition..."
	@if [ -z "$(ESP_ROOT)" ]; then \
		echo "Error: ESP_ROOT not set. Usage: make install ESP_ROOT=/mnt/esp"; \
		exit 1; \
	fi
	mkdir -p $(ESP_ROOT)/EFI/servecosys
	cp $(TARGET_EFI) $(ESP_ROOT)/EFI/servecosys/
	@echo "[OK] Installed to $(ESP_ROOT)/EFI/servecosys/"

# 签名（需要 sbsign）
sign: $(TARGET_EFI)
	@echo "[SIGN] Signing $(TARGET_EFI)..."
	@if [ -z "$(KEY_FILE)" ] || [ -z "$(CERT_FILE)" ]; then \
		echo "Error: KEY_FILE and CERT_FILE required. Usage: make sign KEY_FILE=key.key CERT_FILE=cert.crt"; \
		exit 1; \
	fi
	sbsign --key $(KEY_FILE) --cert $(CERT_FILE) $(TARGET_EFI)
	@echo "[OK] Signed $(TARGET_EFI).signed"

# 帮助
help:
	@echo "ServEcosys Bootloader Build System"
	@echo ""
	@echo "Usage:"
	@echo "  make [ARCH=<arch>]        Build bootloader (default: x86_64)"
	@echo "  make clean                Remove build artifacts"
	@echo "  make install              Install to ESP partition (requires ESP_ROOT)"
	@echo "  make sign                 Sign with Secure Boot key (requires KEY_FILE, CERT_FILE)"
	@echo ""
	@echo "Examples:"
	@echo "  make                      # Build x86_64 version"
	@echo "  make ARCH=arm64           # Build ARM64 version"
	@echo "  make install ESP_ROOT=/mnt/esp"
	@echo "  make sign KEY_FILE=keys/maintainer.key CERT_FILE=keys/maintainer.crt"

.PHONY: all clean install sign help
