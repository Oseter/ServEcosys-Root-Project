# ServEcosys 开发路线图

> 为用户而生，因开源而活

## 根本纲领

**以用户主权为设计原则，以用户体验需求为落地基准，为用户服务。**

这是 ServEcosys 一切开发和决策的出发点和落脚点。

---

## 阶段划分

### Phase 0: 基础框架（当前阶段）✅

**目标：** 建立项目结构和核心代码框架

**已完成：**
- ✅ 项目目录结构
- ✅ Bootloader 框架（UEFI 引导程序）
- ✅ 内核核心框架（权限阶梯、SELinux 钩子）
- ✅ 设备模块集框架（PC/Mobile/Probe）
- ✅ 构建脚本（Linux/Windows）
- ✅ initramfs 最小化框架
- ✅ 密钥生成脚本
- ✅ QEMU 测试脚本
- ✅ 文档（README, INSTALL, ROADMAP）

**待完善：**
- ⏳ 完整的 Linux 内核配置（基于 6.6 LTS）
- ⏳ 真实的 SELinux 策略文件
- ⏳ Btrfs 快照管理工具
- ⏳ 硬件指纹生成算法（SHA256）

---

### Phase 1: 可启动原型（Q2 2026）🔄

**目标：** 实现可启动的最小化系统

**内核：**
- [ ] 集成 Linux 6.6 LTS 源码
- [ ] 编译通过，QEMU 可启动
- [ ] 硬件指纹模块工作
- [ ] SELinux 策略加载成功
- [ ] 设备模块按需加载

**引导：**
- [ ] UEFI 引导程序编译通过
- [ ] Secure Boot 签名支持
- [ ] 内核签名验证
- [ ] 快照配置解析

**用户空间：**
- [ ] initramfs 包含必要工具（BusyBox）
- [ ] systemd 最小化集成
- [ ] Btrfs 挂载脚本
- [ ] 基础 Shell 环境

**测试：**
- [ ] QEMU x86_64 启动成功
- [ ] QEMU ARM64 启动成功
- [ ] 物理机 UEFI 启动测试

---

### Phase 2: 双域隔离（Q3 2026）

**目标：** 实现前后端安全域隔离

**SELinux 策略：**
- [ ] 定义 `sys_dom_t`（SED 后端域）
- [ ] 定义 `uid_dom_t`（UID 前端域）
- [ ] 定义 `app_sandbox_t`（应用沙箱）
- [ ] 实现域间 IPC 策略

**IPC 总线：**
- [ ] 基于 binderfs 的 IPC 实现
- [ ] 权能（capability）令牌系统
- [ ] 单向响应模式
- [ ] 审计日志记录

**后端安全域（SED）：**
- [ ] SELinux 策略管理器
- [ ] 硬件抽象层守护进程
- [ ] 权限裁决服务
- [ ] OIPES 客户端代理

**前端交互域（UID）：**
- [ ] 显示服务
- [ ] 输入管理（键盘/鼠标/触控）
- [ ] 应用沙箱运行时
- [ ] 系统界面层（状态栏、通知）

---

### Phase 3: 权限阶梯（Q4 2026）

**目标：** 实现 0-11 级权限体系

**权限管理：**
- [ ] 进程权限级别标记
- [ ] 权限检查 LSM 钩子
- [ ] BL 解锁机制（4 级）
- [ ] Root 分能力（5 级）
- [ ] SELinux 控制接口（8 级）⭐ 用户控制
- [ ] 引导加载程序控制（11 级）⭐ 用户控制

**签名链：**
- [ ] 用户自签密钥管理
- [ ] 官方信任锚点
- [ ] 自签链验证机制

**安全启动：**
- [ ] UEFI Secure Boot 集成
- [ ] 内核签名验证
- [ ] 模块签名验证
- [ ] 应用签名验证

---

### Phase 4: 系统自愈（Q1 2027）

**目标：** 实现 Btrfs 原子快照和恢复

**Btrfs 集成：**
- [ ] 多子卷布局（@system, @data, @snapshots）
- [ ] 只读 @system 子卷
- [ ] 独立 @data 用户数据卷

**快照管理：**
- [ ] 更新前自动创建快照
- [ ] 快照元数据管理
- [ ] 快照枚举和选择
- [ ] 引导菜单集成

**恢复机制：**
- [ ] 轻症：快照回滚（10 秒）
- [ ] 中症：/dev/recovery 分区重装
- [ ] 重症：底基系统终极恢复

---

### Phase 5: 设备适配（Q2 2027）

**目标：** 支持主流 PC 和移动设备

**PC 模块集：**
- [ ] Intel/AMD 芯片组驱动
- [ ] GPU 驱动（i915, AMDGPU, Nouveau）
- [ ] 网络驱动（Intel, Realtek, Broadcom）
- [ ] 音频驱动（ALSA, PulseAudio）
- [ ] USB 3.x 支持

**移动模块集：**
- [ ] SoC 支持（Qualcomm, MediaTek, Apple Silicon）
- [ ] 触控屏驱动
- [ ] 传感器驱动（IIO）
- [ ] 电源管理
- [ ] 蜂窝网络（4G/5G）

**硬件探测：**
- [ ] 自动硬件指纹生成
- [ ] 设备树编译
- [ ] ACPI 表解析

---

### Phase 6: 应用生态（Q3 2027）

**目标：** 建立应用打包和分发系统

**应用格式：**
- [ ] .ssle 打包工具（ssle-packer）
- [ ] .smle 系统服务打包
- [ ] 权限扫描和签名
- [ ] 应用商店原型

**沙箱运行时：**
- [ ] SELinux 标签自动分配
- [ ] 文件系统命名空间
- [ ] 网络权限控制
- [ ] 设备访问控制

**OIPES 服务：**
- [ ] 身份认证 API
- [ ] 推送通知网关
- [ ] AI 模型推理接口
- [ ] 云存储中继

---

### Phase 7: 开发者工具（Q4 2027）

**目标：** 降低开发门槛

**工具链：**
- [ ] cross-compile 工具链
- [ ] QEMU 多架构仿真
- [ ] 调试工具（GDB, strace）
- [ ] SELinux 审计工具

**文档：**
- [ ] 开发者指南
- [ ] API 文档
- [ ] 教程和示例
- [ ] 贡献指南

**CI/CD：**
- [ ] 自动化构建
- [ ] 自动化测试
- [ ] 镜像生成和发布

---

### Phase 8: 社区建设（2028+）

**目标：** 建立开源社区

- [ ] 官方网站
- [ ] 代码托管（GitHub/GitLab）
- [ ] 邮件列表和论坛
- [ ] 定期发布会
- [ ] 安全审计流程
- [ ] 治理结构

---

## 技术债务

以下问题需要在后续版本解决：

1. **签名验证简化**：当前 bootloader.c 中 `verify_signature()` 返回 TRUE，需实现真实 RSA/ECDSA 验证
2. **SELinux 策略编译**：需要集成 checkpolicy 和 secilc 工具链
3. **硬件指纹算法**：需要实现 SHA256 哈希，而非简单复制
4. **IPC 性能**：binderfs 实现需要优化延迟
5. **文档完整性**：缺少详细的架构设计文档

---

## 依赖项

### 核心依赖
- Linux Kernel 6.6 LTS
- GNU EFI / EDK II
- systemd (minimal)
- BusyBox
- Btrfs tools
- SELinux userspace tools

### 构建依赖
- GCC 9.0+
- Binutils
- Make
- CMake (部分工具)
- OpenSSL

### 测试依赖
- QEMU 6.0+
- OVMF (UEFI firmware)
- libvirt (可选)

---

## 如何贡献

1. Fork 项目仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

---

## 联系

- 项目网站：（待建设）
- 邮件列表：（待创建）
- 代码仓库：https://github.com/Oseter/ServEcosys-Root-Project

---

**ServEcosys — 为用户而生，因开源而活。**

**根本纲领：以用户主权为设计原则，以用户体验需求为落地基准，为用户服务。**
