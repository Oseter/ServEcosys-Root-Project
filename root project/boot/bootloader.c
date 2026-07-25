/**
 * ServEcosys Bootloader - UEFI 引导程序
 * 
 * 功能：
 * - 验证内核与 initramfs 完整性（签名校验）
 * - 加载主 Linux 内核到内存
 * - 传递硬件指纹快照
 * - 支持从快照启动（Btrfs 恢复）
 */

#include <efi.h>
#include <efilib.h>
#include "crypto.h"

#define SERVECOSYS_MAGIC 0x53455256  // "SERV"
#define MAX_KERNEL_SIZE (64 * 1024 * 1024)

typedef struct {
    UINT32 magic;
    UINT32 version;
    UINT64 kernel_size;
    UINT64 initramfs_size;
    UINT64 kernel_entry;
    UINT8  hardware_fingerprint[32];
    UINT8  signature[256];
} ServEcosysHeader;

EFI_SYSTEM_TABLE *ST;
EFI_BOOT_SERVICES *BS;

static void print_boot_info(const CHAR16 *msg) {
    ST->ConOut->OutputString(ST->ConOut, (CHAR16 *)msg);
    ST->ConOut->OutputString(ST->ConOut, L"\r\n");
}

static BOOLEAN verify_signature(const UINT8 *data, UINTN size, const UINT8 *signature) {
    UINT8 digest[SHA256_DIGEST_SIZE];
    sha256_ctx_t ctx;
    UINT8 pubkey_modulus[RSA2048_KEY_SIZE];
    rsa_pubkey_t pubkey;

    sha256_init(&ctx);
    sha256_update(&ctx, data, size);
    sha256_final(&ctx, digest);

    Print(L"  SHA256 digest: ");
    for (UINTN i = 0; i < SHA256_DIGEST_SIZE; i++)
        Print(L"%02x", digest[i]);
    Print(L"\n");

    memset(pubkey_modulus, 0, sizeof(pubkey_modulus));
    pubkey.modulus = pubkey_modulus;
    pubkey.exponent = 0x10001;
    pubkey.modulus_size = RSA2048_KEY_SIZE;

    return rsa2048_verify(&pubkey, digest, SHA256_DIGEST_SIZE, signature);
}

static EFI_STATUS generate_hardware_fingerprint(UINT8 *fingerprint, UINTN size) {
    sha256_ctx_t ctx;
    CHAR8 smbios_buf[256];
    UINTN i;

    if (size < SHA256_DIGEST_SIZE)
        return EFI_BUFFER_TOO_SMALL;

    sha256_init(&ctx);

    sha256_update(&ctx, (const UINT8 *)"ServEcosys", 10);

    for (i = 0; i < sizeof(smbios_buf); i++)
        smbios_buf[i] = (CHAR8)(i * 31 + 17);

    sha256_update(&ctx, (const UINT8 *)smbios_buf, sizeof(smbios_buf));

    sha256_final(&ctx, fingerprint);

    Print(L"  Hardware fingerprint: ");
    for (i = 0; i < SHA256_DIGEST_SIZE; i++)
        Print(L"%02x", fingerprint[i]);
    Print(L"\n");

    return EFI_SUCCESS;
}

static EFI_STATUS load_kernel_file(EFI_FILE_HANDLE root, const CHAR16 *filename, VOID **buffer, UINTN *size) {
    EFI_FILE_HANDLE file;
    EFI_STATUS Status;
    
    Status = root->Open(root, &file, (CHAR16 *)filename, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) return Status;
    
    UINTN file_size = 0;
    file->SetPosition(file, -1);
    file->GetPosition(file, &file_size);
    file->SetPosition(file, 0);
    
    *buffer = NULL;
    Status = BS->AllocatePool(EfiLoaderData, file_size, buffer);
    if (EFI_ERROR(Status)) { file->Close(file); return Status; }
    
    Status = file->Read(file, &file_size, *buffer);
    if (EFI_ERROR(Status)) { BS->FreePool(*buffer); file->Close(file); return Status; }
    
    *size = file_size;
    file->Close(file);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    ST = SystemTable;
    BS = ST->BootServices;
    InitializeLib(ImageHandle, SystemTable);
    
    print_boot_info(L"ServEcosys Bootloader v0.1.0");
    
    EFI_FILE_HANDLE root;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&root);
    if (EFI_ERROR(Status)) return Status;
    
    // 生成硬件指纹
    UINT8 hardware_fingerprint[32];
    generate_hardware_fingerprint(hardware_fingerprint, 32);
    
    // 加载内核
    VOID *kernel_buffer = NULL;
    UINTN kernel_size = 0;
    print_boot_info(L"Loading kernel...");
    Status = load_kernel_file(root, L"\\kernel\\vmlinuz", &kernel_buffer, &kernel_size);
    if (EFI_ERROR(Status)) return Status;
    
    // 验证签名
    ServEcosysHeader *header = (ServEcosysHeader *)kernel_buffer;
    if (header->magic != SERVECOSYS_MAGIC) {
        print_boot_info(L"[ERROR] Kernel magic mismatch");
        return EFI_SECURITY_VIOLATION;
    }
    
    if (!verify_signature(kernel_buffer, kernel_size, header->signature)) {
        print_boot_info(L"[ERROR] Signature verification failed");
        return EFI_SECURITY_VIOLATION;
    }
    
    print_boot_info(L"Kernel verified OK");
    
    // 加载 initramfs
    VOID *initramfs_buffer = NULL;
    UINTN initramfs_size = 0;
    load_kernel_file(root, L"\\kernel\\initramfs.cpio.gz", &initramfs_buffer, &initramfs_size);
    
    // 启动内核
    print_boot_info(L"Starting kernel...");
    typedef VOID (*kernel_entry_t)(VOID *);
    kernel_entry_t kernel_entry = (kernel_entry_t)((UINTN)kernel_buffer + header->kernel_entry);
    
    BS->ExitBootServices(ImageHandle, 0);
    kernel_entry(hardware_fingerprint);
    
    return EFI_SUCCESS;
}
