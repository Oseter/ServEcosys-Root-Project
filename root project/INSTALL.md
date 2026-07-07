# ServEcosys 安装与编译指南

## 系统要求

### 最低要求
- CPU: x86_64 或 ARM64
- 内存：4GB（编译），512MB（运行）
- 存储：10GB 可用空间
- 编译器：GCC 9.0+ 或 Clang 12+

### 推荐配置
- CPU: 4 核以上
- 内存：16GB
- 存储：SSD，30GB 可用空间

---

## 1. Linux 环境安装

### 1.1 Ubuntu/Debian

```bash
# 安装基础编译工具
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libncurses-dev \
    bison \
    flex \
    libssl-dev \
    libelf-dev \
    binutils-dev \
    libfdt-dev \
    device-tree-compiler \
    cpio \
    kmod \
    git \
    wget

# 安装交叉编译工具链（可选，用于多架构编译）
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    gcc-arm-linux-gnueabihf \
    gcc-riscv64-linux-gnu
```

### 1.2 Fedora/RHEL

```bash
sudo dnf install -y \
    gcc make binutils \
    ncurses-devel \
    bison flex \
    openssl-devel \
    elfutils-libelf-devel \
    dtc \
    cpio \
    kmod \
    git
```

### 1.3 Arch Linux

```bash
sudo pacman -S --needed \
    base-devel \
    ncurses \
    bison \
    flex \
    openssl \
    elfutils \
    dtc \
    cpio \
    kmod \
    git
```

---

## 2. Windows 环境安装

### 2.1 方案一：WSL2（推荐）

```powershell
# 启用 WSL
wsl --install -d Ubuntu-22.04

# 在 WSL 中执行 Linux 安装步骤
wsl
# 然后执行上面的 Ubuntu/Debian 安装命令
```

### 2.2 方案二：MinGW-w64

```powershell
# 使用 Chocolatey 安装
choco install mingw msys2 git

# 或使用 Scoop
scoop install mingw git
```

**注意：** Windows 原生编译仅支持部分工具链，完整编译建议使用 WSL2。

---

## 3. UEFI 引导程序编译

### 3.1 安装 gnuefi 工具链

```bash
# Ubuntu/Debian
sudo apt-get install -y gnu-efi

# 或从源码编译
git clone https://github.com/rhboot/gnu-efi.git
cd gnu-efi
make
sudo make install
```

### 3.2 编译引导程序

```bash
cd servecosys/boot

# 编译为 UEFI 应用
gcc -I/usr/include/efi \
    -I/usr/include/efi/x86_64 \
    -I/usr/include/efi/protocol \
    -fno-stack-protector \
    -fpic \
    -fshort-wchar \
    -mno-red-zone \
    -Wall \
    -DEFI_FUNCTION_WRAPPER \
    -c bootloader.c -o bootloader.o

ld -nostdlib -znocombreloc -T /usr/lib/efi/crt0-efi-x86_64.lds \
   bootloader.o -o bootloader.so \
   -L/usr/lib -lefi -lgnuefi \
   -O2 -Bsymbolic

# 生成 EFI 可执行文件
objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
        -j .rel -j .rela -j .reloc \
        --target=efi-app-x86_64 \
        bootloader.so bootloader.efi
```

### 3.3 签名（可选，用于安全启动）

```bash
# 生成密钥对（项目维护者 + 审计方共管）
openssl genrsa -out servecosys.key 4096
openssl req -new -key servecosys.key -out servecosys.csr -subj "/CN=ServEcosys"
openssl x509 -req -days 365 -in servecosys.csr -signkey servecosys.key -out servecosys.crt

# 签名 EFI 应用
# sbsign --key servecosys.key --cert servecosys.crt bootloader.efi
```

---

## 4. 内核编译

### 4.1 准备内核源码

```bash
cd servecosys

# 下载 Linux LTS 源码
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.tar.xz
tar -xf linux-6.6.tar.xz
mv linux-6.6 kernel/linux-src
```

### 4.2 配置内核

```bash
cd kernel/linux-src

# 使用 ServEcosys 最小化配置
cp ../core/servecosys_defconfig .config

# 或使用 menuconfig 自定义
make menuconfig
```

### 4.3 编译内核

```bash
# 编译内核镜像
make -j$(nproc) bzImage

# 编译模块
make -j$(nproc) modules

# 安装模块
sudo make modules_install INSTALL_MOD_PATH=../build/modules

# 复制产物
cp arch/x86/boot/bzImage ../build/vmlinuz
```

### 4.4 交叉编译（ARM64）

```bash
cd kernel/linux-src

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

make servecosys_arm64_defconfig
make -j$(nproc) Image.gz
make -j$(nproc) modules

cp arch/arm64/boot/Image.gz ../build/vmlinuz-arm64
```

---

## 5. Initramfs 生成

### 5.1 创建 initramfs 结构

```bash
cd servecosys/scripts

mkdir -p initramfs/{bin,sbin,lib,usr,etc,proc,sys,dev,mnt}

# 复制必要工具
cp /bin/busybox initramfs/bin/
cd initramfs/bin
ln -s busybox sh
ln -s busybox mount
ln -s busybox mkdir
ln -s busybox cat
# ... 其他必要命令
```

### 5.2 创建 init 脚本

```bash
cat > initramfs/init << 'EOF'
#!/bin/sh

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev

# 加载硬件指纹模块
modprobe servecosys_probe

# 挂载 Btrfs 根
mount -o subvol=@system /dev/sda2 /mnt

# 切换到真实根
exec switch_root /mnt /sbin/init
EOF

chmod +x initramfs/init
```

### 5.3 打包 initramfs

```bash
cd servecosys/scripts/initramfs
find . | cpio -H newc -o | gzip > ../build/initramfs.cpio.gz
```

---

## 6. 签名内核

### 6.1 生成共管密钥

```bash
cd servecosys/scripts

# 项目维护者密钥
openssl genrsa -out maintainer.key 4096
openssl req -new -key maintainer.key -out maintainer.csr -subj "/CN=ServEcosys Maintainer"
openssl x509 -req -days 3650 -in maintainer.csr -signkey maintainer.key -out maintainer.crt

# 审计方密钥
openssl genrsa -out auditor.key 4096
openssl req -new -key auditor.key -out auditor.csr -subj "/CN=ServEcosys Auditor"
openssl x509 -req -days 3650 -in auditor.csr -signkey auditor.key -out auditor.crt
```

### 6.2 签名内核镜像

```bash
# 使用 kexec-tools 的脚本签名
./scripts/sign_kernels.sh ../build/vmlinuz

# 或手动签名
openssl dgst -sha256 -sign maintainer.key -out ../build/vmlinuz.sig ../build/vmlinuz
```

---

## 7. 使用构建脚本

### 7.1 Linux

```bash
cd servecosys

# 编译全部
./scripts/build_kernel.sh all

# 仅编译 PC 模块
./scripts/build_kernel.sh pc

# 仅生成 initramfs
./scripts/build_kernel.sh initramfs

# 签名内核
./scripts/build_kernel.sh sign
```

### 7.2 Windows (PowerShell)

```powershell
cd servecosys

# 编译全部
.\scripts\build.ps1 -Target all

# 仅编译移动模块
.\scripts\build.ps1 -Target mobile
```

---

## 8. 验证编译产物

```bash
cd servecosys/build

# 检查文件
ls -lh

# 验证签名
openssl dgst -sha256 -verify maintainer.crt -signature vmlinuz.sig vmlinuz

# 检查 initramfs 内容
lsinitcpio initramfs.cpio.gz | head -20
```

### 预期输出

```
vmlinuz              # 内核镜像 (~10MB)
vmlinuz.sig          # 签名文件
initramfs.cpio.gz    # 初始化文件系统 (~5MB)
bootloader.efi       # UEFI 引导程序
modules/             # 内核模块目录
```

---

## 9. QEMU 测试

### 9.1 x86_64 测试

```bash
qemu-system-x86_64 \
    -kernel servecosys/build/vmlinuz \
    -initrd servecosys/build/initramfs.cpio.gz \
    -append "console=ttyS0" \
    -nographic \
    -m 2G \
    -smp 2
```

### 9.2 ARM64 测试

```bash
qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -kernel servecosys/build/vmlinuz-arm64 \
    -initrd servecosys/build/initramfs-arm64.cpio.gz \
    -append "console=ttyAMA0" \
    -nographic \
    -m 2G \
    -smp 2
```

---

## 10. 故障排查

### 常见问题

**1. 编译错误：`efi.h: No such file or directory`**
```bash
# 安装 gnu-efi 开发文件
sudo apt-get install gnu-efi libgnuefi-dev
```

**2. 链接错误：`cannot find -lefi`**
```bash
# 检查库路径
export LDFLAGS="-L/usr/lib/efi"
```

**3. 签名验证失败**
```bash
# 确认使用正确的密钥
openssl x509 -in maintainer.crt -text -noout
```

**4. QEMU 启动卡住**
```bash
# 添加调试参数
-append "console=ttyS0 debug loglevel=7"
```

---

## 11. 下一步

编译完成后，参考：
- [`README.md`](README.md) - 项目架构说明
- [`ROADMAP.md`](ROADMAP.md) - 开发路线图
- [`kernel/core/ARCHITECTURE.md`](kernel/core/ARCHITECTURE.md) - 内核架构说明

---

**ServEcosys — 为用户而生，因开源而活。**

**根本纲领：以用户为中心，为用户服务。**
