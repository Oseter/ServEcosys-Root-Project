/**
 * ServEcosys Base System - 底基系统实现
 */

#include "base.h"

base_system_t g_base_system;

EFI_STATUS base_system_init(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab) {
    g_base_system.systab = systab;
    g_base_system.boot_services = systab->BootServices;
    g_base_system.runtime_services = systab->RuntimeServices;
    
    // 初始化控制台
    g_base_system.console.print = systab->ConOut->OutputString;
    
    return EFI_SUCCESS;
}

UINT64 base_mmio_read(UINT64 addr) {
    volatile UINT64 *ptr = (UINT64 *)(UINTN)addr;
    return *ptr;
}

VOID base_mmio_write(UINT64 addr, UINT64 value) {
    volatile UINT64 *ptr = (UINT64 *)(UINTN)addr;
    *ptr = value;
}

EFI_STATUS base_load_kernel(EFI_FILE_HANDLE root, VOID **kernel, UINTN *size) {
    EFI_FILE_HANDLE file;
    EFI_STATUS Status;
    
    Status = root->Open(root, &file, L"\\kernel\\vmlinuz", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) return Status;
    
    UINTN file_size = 0;
    file->SetPosition(file, -1);
    file->GetPosition(file, &file_size);
    file->SetPosition(file, 0);
    
    Status = g_base_system.boot_services->AllocatePool(EfiLoaderData, file_size, kernel);
    if (EFI_ERROR(Status)) { file->Close(file); return Status; }
    
    Status = file->Read(file, &file_size, *kernel);
    if (EFI_ERROR(Status)) {
        g_base_system.boot_services->FreePool(*kernel);
        file->Close(file);
        return Status;
    }
    
    *size = file_size;
    file->Close(file);
    return EFI_SUCCESS;
}

EFI_STATUS base_boot_system(VOID *kernel, VOID *initramfs) {
    // TODO: 跳转到内核入口
    // 清理 EFI 环境，传递 initramfs
    return EFI_SUCCESS;
}

EFI_STATUS base_restore_system(EFI_FILE_HANDLE root) {
    // TODO: 从 Btrfs 快照恢复系统
    // 1. 枚举可用快照
    // 2. 挂载选择的快照
    // 3. 重新部署系统
    return EFI_SUCCESS;
}
