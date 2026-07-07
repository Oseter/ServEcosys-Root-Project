# 原生第三方应用 (.ssle)

应用沙箱运行时目录。

## .ssle 格式

**.ssle** = Subsystem Module Loadable Executable

- 原子化应用单元
- 启动 ELF + 多个业务 ELF 压缩合并
- 包级权限裁决（非 per-ELF）
- 同包内 ELF 共享 SELinux 标签

## 打包工具

```bash
# 打包应用
ssle-packer \
    --name com.example.myapp \
    --version 1.0.0 \
    --entry main.elf \
    --include data.elf,worker.elf \
    --permissions network,storage \
    --output myapp.ssle
```

## 权限授予

用户在安装时授予权限：

```json
{
  "app_id": "com.example.myapp",
  "permissions": {
    "network": true,
    "storage": "scoped",
    "camera": false,
    "location": false
  }
}
```

## 文件结构

```
frontend/apps/
├── README.md
├── system/              # 系统应用
│   ├── settings.ssle
│   └── file_manager.ssle
└── third_party/         # 第三方应用
    └── (用户安装的应用)
```

## 开发状态

⏳ 待实现

---

**ServEcosys — 为用户而生，因开源而活。**
