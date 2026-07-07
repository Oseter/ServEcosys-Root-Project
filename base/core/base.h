/**
 * ServEcosys Base System - 底基系统核心
 * 
 * 最小化 UEFI 运行时环境，提供：
 * - 直接硬件访问能力
 * - 操作系统加载与管理
 * - 系统恢复功能
 */

#ifndef SERVECOSYS_BASE_H
#define SERVECOSYS_BASE_H

#include <efi.h>
#include <efilib.h>

// 底基系统版本
#define BASE_SYSTEM_VERSION "0.1.0"

// 硬件访问接口
typedef struct {
    UINT64 (*mmio_read)(UINT64 addr);
    VOID (*mmio_write)(UINT64 addr, UINT64 value);
    VOID (*io_read)(UINT16 port, UINT8 *value);
    VOID (*io_write)(UINT16 port, UINT8 value);
} hardware_ops_t;

// 系统恢复接口
typedef struct {
    EFI_STATUS (*load_kernel)(EFI_FILE_HANDLE root, VOID **kernel, UINTN *size);
    EFI_STATUS (*verify_signature)(VOID *data, UINTN size, UINT8 *signature);
    EFI_STATUS (*boot_system)(VOID *kernel, VOID *initramfs);
    EFI_STATUS (*restore_from_snapshot)(EFI_FILE_HANDLE root, CHAR16 *snapshot_id);
} system_ops_t;

// 控制台接口
typedef struct {
    VOID (*print)(const CHAR16 *str);
    VOID (*println)(const CHAR16 *str);
    EFI_STATUS (*read_line)(CHAR16 *buffer, UINTN max_len);
} console_ops_t;

// 全局系统结构
typedef struct {
    hardware_ops_t hardware;
    system_ops_t system;
    console_ops_t console;
    EFI_SYSTEM_TABLE *systab;
    EFI_BOOT_SERVICES *boot_services;
    EFI_RUNTIME_SERVICES *runtime_services;
} base_system_t;

extern base_system_t g_base_system;

// 初始化函数
EFI_STATUS base_system_init(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab);

// 硬件访问函数
UINT64 base_mmio_read(UINT64 addr);
VOID base_mmio_write(UINT64 addr, UINT64 value);

// 系统加载函数
EFI_STATUS base_load_kernel(EFI_FILE_HANDLE root, VOID **kernel, UINTN *size);
EFI_STATUS base_boot_system(VOID *kernel, VOID *initramfs);

// 恢复功能
EFI_STATUS base_restore_system(EFI_FILE_HANDLE root);

#endif // SERVECOSYS_BASE_H
