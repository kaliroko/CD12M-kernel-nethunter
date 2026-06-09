# Kali NetHunter Kernel 自动编译工作流

## 📋 工作流说明

此工作流自动编译 CD12M 设备的 Kali NetHunter 内核。

## 🚀 触发条件

### 自动触发：
1. **代码推送** - 修改以下文件时自动编译：
   - `arch/arm64/**` - ARM64架构文件
   - `drivers/**` - 驱动程序
   - `kernel/**` - 内核核心

2. **定时编译** - 每周日凌晨2点 UTC 自动编译

3. **标签发布** - 推送新 tag（如 `v1.0.0`）时自动编译

4. **手动触发** - 在 GitHub Actions 页面手动运行

### 手动触发方式：
1. 进入项目 → **Actions**
2. 选择 **"Build Kali NetHunter Kernel for CD12M"**
3. 点击 **"Run workflow"**
4. 选择 build variant（release/debug/test）
5. 点击 **"Run workflow"**

---

## 📦 编译产物

编译完成后生成以下文件：

| 文件 | 说明 |
|------|------|
| `kernel-image.gz` | 压缩的内核镜像 |
| `*.dtb` | 设备树文件 |
| `SHA256SUMS` | SHA256 校验和 |
| `MD5SUMS` | MD5 校验和 |
| `BUILD_INFO.txt` | 构建信息 |

### 下载方式：
1. 进入项目 → **Actions**
2. 选择最新的工作流运行
3. 点击 **"Artifacts"** 下载 `kernel-build-release`

---

## ⚙️ 工作流步骤

### 1. 构建环境设置
```bash
# 安装编译工具链和依赖
build-essential, libncurses-dev, bison, flex, libssl-dev
gcc-aarch64-linux-gnu, device-tree-compiler
```

### 2. 内核配置
```bash
make cd12m_defconfig
```

### 3. 内核编译
```bash
make -j$(nproc)  # 多线程编译
```

### 4. 设备树编译
```bash
make dtbs -j$(nproc)
```

### 5. 模块编译
```bash
make modules -j$(nproc)
```

### 6. 产物生成
- 生成校验和
- 创建构建信息
- 打包为 Release

### 7. 测试验证
- 验证内核镜像完整性
- 验证校验和

### 8. 通知发布
- 自动发布编译完成通知
- 支持 Reddit, Twitter, Telegram, Discord

---

## 🔧 配置修改

### 修改编译并行度
编辑 `.github/workflows/build-kernel.yml`，找到：
```yaml
make -j$(nproc)
```
改为：
```yaml
make -j8  # 固定8线程
```

### 修改编译时间表
编辑触发条件（cron 表达式）：
```yaml
schedule:
  - cron: '0 2 * * 0'  # 每周日凌晨2点
```

常用时间表：
```
# 每天凌晨2点
- cron: '0 2 * * *'

# 每周一上午10点
- cron: '0 10 * * 1'

# 每月1号上午10点
- cron: '0 10 1 * *'
```

### 修改内核配置
```bash
# 本地修改
make menuconfig

# 保存为新的 defconfig
cp .config arch/arm64/configs/cd12m_defconfig
git add arch/arm64/configs/cd12m_defconfig
git commit -m "chore: Update kernel config"
git push
```

---

## 📊 工作流状态查看

### 在 GitHub 上查看：
1. 项目 → **Actions**
2. 选择 **"Build Kali NetHunter Kernel for CD12M"**
3. 查看最新运行记录

### 查看构建日志：
1. 点击工作流运行
2. 点击 **"build-kernel"** 作业
3. 展开各步骤查看详细日志

---

## ✅ 编译成功标志

编译成功时会输出：
```
✅ Kernel build completed successfully!
✓ Kernel image found
✓ Verifying checksums...
```

---

## ❌ 故障排除

### 编译失败

**问题**: 工作流显示红色 ❌

**解决方案**：
1. 点击工作流进入详情
2. 查看 **"Build kernel"** 步骤的日志
3. 常见错误：
   - 缺少依赖 → 检查 `Set up build environment` 步骤
   - 配置错误 → 检查 `make cd12m_defconfig`
   - 编译错误 → 查看 `build.log`

### 产物下载不了

**问题**: Artifacts 为空

**解决方案**：
1. 检查编译是否真的成功
2. 查看 `Generate kernel image` 步骤
3. 确保输出文件存在

### 通知未发送

**问题**: 编译成功但没有收到通知

**解决方案**：
1. 检查 GitHub Secrets 是否配置正确
2. 检查平台 API token 是否有效
3. 查看 `Post build notification` 步骤日志

---

## 🔐 安全建议

1. **不要提交密钥** - 使用 GitHub Secrets
2. **定期更新密钥** - 定期更新 API tokens
3. **限制访问权限** - 仅授予必要的 workflow 权限
4. **审计日志** - 定期检查工作流执行记录

---

## 📈 性能优化

### 加快编译速度：
1. 使用更高的并行度：`make -j16`
2. 使用 ccache：`apt install ccache`
3. 使用 Ninja 构建系统

### 减少工作流时间：
1. 跳过不必要的步骤
2. 合并测试和编译步骤
3. 使用缓存

---

## 📝 后续改进

- [ ] 支持多设备编译
- [ ] 添加代码质量检查
- [ ] 支持增量编译
- [ ] 自动发布到 Release
- [ ] 通知到多个平台

---

## 📖 相关文档

- [Linux 内核编译指南](https://www.kernel.org/doc/html/latest/kbuild/)
- [Kali NetHunter 文档](https://www.kali.org/docs/nethunter/)
- [GitHub Actions 文档](https://docs.github.com/en/actions)

---

## 🤝 贡献

如需改进此工作流，欢迎提交 Pull Request！
