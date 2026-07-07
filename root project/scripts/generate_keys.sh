#!/bin/bash
#
# ServEcosys Key Generation Script
#
# 生成项目维护者和独立审计方的共管密钥对
# 用于：
# - 内核签名
# - 引导程序签名（Secure Boot）
# - SELinux 策略签名
# - 应用包签名（.ssle/.smle）
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KEYS_DIR="$SCRIPT_DIR/keys"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# 生成单个密钥对
generate_keypair() {
    local name="$1"
    local cn="$2"
    local org="$3"
    local days="${4:-3650}"
    
    log_step "Generating $name key pair..."
    
    # 生成 RSA 密钥
    openssl genrsa -out "$KEYS_DIR/$name.key" 4096
    chmod 600 "$KEYS_DIR/$name.key"
    
    # 生成 CSR
    openssl req -new -key "$KEYS_DIR/$name.key" \
        -out "$KEYS_DIR/$name.csr" \
        -subj "/CN=$cn/O=$org/C=CN"
    
    # 自签名证书
    openssl x509 -req -days "$days" \
        -in "$KEYS_DIR/$name.csr" \
        -signkey "$KEYS_DIR/$name.key" \
        -out "$KEYS_DIR/$name.crt"
    
    log_info "$name key pair generated"
}

# 生成 ECDSA 密钥（用于应用签名）
generate_ecdsa_keypair() {
    local name="$1"
    local cn="$2"
    local org="$3"
    
    log_step "Generating $name ECDSA key pair..."
    
    # 生成 EC 密钥
    openssl ecparam -genkey -name prime256v1 -out "$KEYS_DIR/$name_ec.key"
    chmod 600 "$KEYS_DIR/$name_ec.key"
    
    # 生成 CSR
    openssl req -new -key "$KEYS_DIR/$name_ec.key" \
        -out "$KEYS_DIR/$name_ec.csr" \
        -subj "/CN=$cn/O=$org/C=CN"
    
    # 自签名证书
    openssl x509 -req -days 3650 \
        -in "$KEYS_DIR/$name_ec.csr" \
        -signkey "$KEYS_DIR/$name_ec.key" \
        -out "$KEYS_DIR/$name_ec.crt"
    
    log_info "$name ECDSA key pair generated"
}

# 生成 Secure Boot 密钥（用于 UEFI 签名）
generate_secure_boot_keys() {
    log_step "Generating Secure Boot keys..."
    
    # 平台密钥 (PK)
    openssl req -x509 -sha256 -newkey rsa:4096 \
        -keyout "$KEYS_DIR/PK.key" \
        -out "$KEYS_DIR/PK.crt" \
        -days 3650 \
        -nodes \
        -subj "/CN=ServEcosys Platform Key/O=ServEcosys Project/C=CN"
    
    # 密钥交换密钥 (KEK)
    openssl req -x509 -sha256 -newkey rsa:4096 \
        -keyout "$KEYS_DIR/KEK.key" \
        -out "$KEYS_DIR/KEK.crt" \
        -days 3650 \
        -nodes \
        -subj "/CN=ServEcosys Key Exchange Key/O=ServEcosys Project/C=CN"
    
    # 数据库密钥 (db) - 用于签名应用
    openssl req -x509 -sha256 -newkey rsa:4096 \
        -keyout "$KEYS_DIR/db.key" \
        -out "$KEYS_DIR/db.crt" \
        -days 3650 \
        -nodes \
        -subj "/CN=ServEcosys Signature Database/O=ServEcosys Project/C=CN"
    
    log_info "Secure Boot keys generated"
}

# 主函数
main() {
    log_info "ServEcosys Key Generation System"
    log_info "Output directory: $KEYS_DIR"
    log_info ""
    
    # 创建密钥目录
    mkdir -p "$KEYS_DIR"
    chmod 700 "$KEYS_DIR"
    
    # 检查是否已存在密钥
    if [ -f "$KEYS_DIR/maintainer.key" ]; then
        log_warn "Keys already exist in $KEYS_DIR"
        echo ""
        echo "Existing keys:"
        ls -la "$KEYS_DIR"/*.key 2>/dev/null || true
        echo ""
        read -p "Overwrite existing keys? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "Aborted"
            exit 0
        fi
        log_warn "Removing existing keys..."
        rm -f "$KEYS_DIR"/*
    fi
    
    # 生成密钥对
    
    # 1. 项目维护者密钥（RSA 4096）
    generate_keypair "maintainer" \
        "ServEcosys Maintainer" \
        "ServEcosys Project" \
        3650
    
    # 2. 独立审计方密钥（RSA 4096）
    generate_keypair "auditor" \
        "Independent Security Auditor" \
        "ServEcosys Security Audit" \
        3650
    
    # 3. 内核模块签名密钥
    generate_keypair "modules" \
        "ServEcosys Kernel Modules" \
        "ServEcosys Project" \
        3650
    
    # 4. 应用签名密钥（ECDSA，用于 .ssle/.smle）
    generate_ecdsa_keypair "apps" \
        "ServEcosys Applications" \
        "ServEcosys Project"
    
    # 5. Secure Boot 密钥（可选）
    echo ""
    read -p "Generate Secure Boot keys? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        generate_secure_boot_keys
    fi
    
    # 显示生成的密钥
    echo ""
    log_step "Generated keys:"
    ls -la "$KEYS_DIR/"
    
    echo ""
    log_info "Key generation complete!"
    echo ""
    echo "IMPORTANT: Store these keys securely!"
    echo "  - Maintainer key: For kernel and critical system signing"
    echo "  - Auditor key: For independent security verification"
    echo "  - Modules key: For kernel module signing"
    echo "  - Apps key: For application package signing"
    echo ""
    echo "Recommended: Move private keys to offline storage or HSM"
    echo ""
}

main "$@"
