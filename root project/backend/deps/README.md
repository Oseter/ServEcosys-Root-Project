# ServEcosys Backend Dependencies (.smle)

供 `.smle` 系统服务使用的依赖库。

```
backend/deps/
├── crypto/     SHA256 / HMAC / RSA
├── log/        审计日志
├── ipc/        域间通信
├── perm/       权限阶梯 0-11
├── selinux/    SELinux 策略管理
├── hal/        硬件抽象层
├── btrfs/      Btrfs 快照管理
└── oipes-net/  OIPES 网络客户端
```

构建：`make -C backend/deps all`
