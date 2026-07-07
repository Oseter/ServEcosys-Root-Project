# 权限控制台与 SELinux

ServEcosys 安全策略执行点。

## 权限阶梯 (0-11 级)

| 级别 | 名称 | 权限说明 |
|------|------|----------|
| 0 | READONLY | 只读/只写/只执行 |
| 1 | SANDBOX | 应用沙盒 |
| 2 | USER | 普通用户/系统应用 |
| 3 | DEBUG | 进阶调试 |
| 4 | BL_UNLOCK | BL 解锁/特权文件 |
| 5 | ROOT_SPLIT | Root 分能力/自定义恢复 |
| 6 | MODULE_ROOT | 模块加载 Root |
| 7 | KERNEL_ROOT | 内核加载 Root |
| 8 | SELINUX | SELinux 控制 |
| 9 | KMOD_LOAD | 内核模块加载 |
| 10 | CUSTOM_KERNEL | 自定义内核 |
| 11 | BOOTLOADER | 引导加载程序/启动链 |

## SELinux 策略文件

```
backend/security/
├── README.md
├── servecosys.te      # 类型增强文件
├── servecosys.fc      # 文件上下文
├── servecosys.if      # 接口文件
└── keys/              # 签名密钥（共管）
    ├── maintainer.key
    └── auditor.key
```

## 关键设计

- **4 级 BL 解锁**：自由之门，用户可自签策略
- **8 级和 11 级共管**：项目维护者 + 独立审计方，单方无法篡改
- **自签链限制**：无法连接 OIPES 高等级服务（技术必然，非惩罚）

## 开发状态

⏳ 待实现

---

**ServEcosys — 为用户而生，因开源而活。**
