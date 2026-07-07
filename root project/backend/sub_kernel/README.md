# 后端子内核 (Sub-Kernel)

后端安全域 (SED) 的子内核组件。

## 职责

- SELinux 策略执行
- 硬件抽象与驱动管理
- 权限判定与审计
- OIPES 客户端代理

## 文件结构

```
backend/sub_kernel/
├── README.md           # 本文件
├── selinux_manager.c   # SELinux 策略管理器
├── hal_manager.c       # 硬件抽象层
├── permission裁决.c    # 权限裁决服务
└── oipes_client.c      # OIPES 客户端
```

## 开发状态

⏳ 待实现

## 接口规范

### IPC 接口

前端域通过 IPC 总线发起请求：

```c
typedef struct {
    u32 request_id;
    u32 caller_perm_level;
    u8 capability_token[16];
    u8 payload[];
} ipc_request_t;

typedef struct {
    u32 request_id;
    u32 result_code;
    u8 payload[];
} ipc_response_t;
```

### SELinux 上下文

- `sys_dom_t` - 后端安全域主上下文
- `uid_dom_t` - 前端交互域上下文
- `app_sandbox_t` - 应用沙箱上下文

## 下一步

1. 实现 SELinux 策略加载器
2. 实现 IPC 总线（binderfs）
3. 实现权限裁决逻辑
4. 集成 OIPES 客户端

---

**ServEcosys — 为用户而生，因开源而活。**
