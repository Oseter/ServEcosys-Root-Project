# 恢复文件区 (Factory Image)

系统恢复镜像和工具。

## 恢复机制

### 轻症 - 快照回滚 (10 秒)
- 引导程序选择上一个健康快照
- 不触及 @data 用户数据
- 适用：更新滚挂、驱动炸、策略写崩

### 中症 - Recovery 分区重装
- 从 `/dev/recovery` 独立隐藏分区引导
- 执行完整系统重装
- 保留用户数据子卷 (@data)

### 重症 - 底基系统终极恢复
- 从底基系统（UEFI 层）启动
- 直接访问存储设备
- 手动修复分区表、重刷固件
- 可从 USB/SD 卡等外部介质加载

## 文件结构

```
restore/
├── README.md
├── factory_image/       # 工厂镜像
│   ├── system.img      # 系统镜像
│   └── recovery.img    # Recovery 分区镜像
├── tools/               # 恢复工具
│   ├── flash.sh        # 刷写脚本
│   └── repair.sh       # 修复脚本
└── keys/               # 恢复密钥
    └── recovery.key
```

## Btrfs 子卷布局

```
@system          # 系统只读基准
@data            # 用户数据（独立）
@snapshots       # 自动快照
  ├── snap_001
  ├── snap_002
  └── ...
```

## 开发状态

⏳ 待实现

---

**ServEcosys — 为用户而生，因开源而活。**
