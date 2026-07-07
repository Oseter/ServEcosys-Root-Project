# 贡献指南

感谢你对 ServEcosys Root Project 的关注！本项目以**以用户为中心，为用户服务**，欢迎任何形式的贡献。

## 行为准则

- 尊重所有贡献者，保持专业和建设性的交流
- 讨论聚焦技术问题，避免人身攻击
- 遵守项目 GPL v2 许可协议

## 如何贡献

### 报告 Bug

1. 在 [GitHub Issues](https://github.com/Oseter/ServEcosys-Root-Project/issues) 中创建新 Issue
2. 描述清晰：复现步骤、预期行为、实际行为、环境信息
3. 如涉及安全漏洞，请勿公开提交，直接联系项目维护者

### 提交代码

1. **Fork** 本仓库
2. 创建功能分支：`git checkout -b feature/your-feature-name`
3. 遵循代码风格：
   - C 代码：Linux kernel 风格（`linux/scripts/checkpatch.pl`）
   - Shell 脚本：POSIX 兼容，使用 `shellcheck` 检查
   - 文档：中文白皮书体或简洁 Markdown
4. 提交前确保编译通过（如适用）
5. 提交 commit：`git commit -m "简要描述改动"`
6. 推送到你的 Fork：`git push origin feature/your-feature-name`
7. 创建 Pull Request，描述改动内容和动机

### Pull Request 要求

- PR 标题简洁明了
- 描述中说明：改了什么、为什么改、如何验证
- 涉及权限模型 / SELinux 策略的改动需额外说明安全影响
- 涉及 kernel/ 下 .c 文件的改动需附编译验证结果

### 文档贡献

- 白皮书段：中文正式语，白皮书体
- repo 文档：简洁，对标白皮书对应章节
- 术语规范：ServEcosys / SED / UID / .smle / .ssle / OIPES / BL / AOSP 不翻译

## 开发环境

参考 [INSTALL.md](servecosys/INSTALL.md) 设置编译环境。

## 项目结构

参考 [PROJECT_STRUCTURE.md](servecosys/PROJECT_STRUCTURE.md) 了解项目目录布局。

## 许可证

本项目采用 [GPL v2](LICENSE) 许可。贡献代码即表示你同意在此许可下分发你的贡献。
