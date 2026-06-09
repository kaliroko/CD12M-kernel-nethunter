# 🤖 CD12M-kernel-nethunter 自动化机器人发帖指南

## 目录
- [快速开始](#快速开始)
- [支持平台](#支持平台)
- [配置指南](#配置指南)
- [发帖内容](#发帖内容)
- [自动化工作流](#自动化工作流)
- [故障排除](#故障排除)

---

## 快速开始

### 安装依赖
```bash
pip install -r requirements.txt
```

### 配置环境变量
```bash
cp .env.example .env
# 编辑 .env 文件，填入各平台的API密钥和账号信息
```

### 运行机器人
```bash
# 一次性发帖到所有平台
python bot_posting.py --all

# 发帖到特定平台
python bot_posting.py --platform reddit
python bot_posting.py --platform twitter
```

---

## 支持平台

| 平台 | 状态 | 配置文件 | 说明 |
|------|------|---------|------|
| **Reddit** | ✅ | `config/reddit.json` | 自动发帖到多个子版块 |
| **Twitter/X** | ✅ | `config/twitter.json` | 定时发推特 |
| **微博** | ✅ | `config/weibo.json` | 支持中文发帖 |
| **Telegram** | ✅ | `config/telegram.json` | 发送到频道和群组 |
| **Discord** | ✅ | `config/discord.json` | Webhook 发帖 |
| **知乎** | ✅ | `config/zhihu.json` | 自动发布内容 |
| **掘金** | ✅ | `config/juejin.json` | 技术社区发帖 |

---

## 配置指南

### 1. Reddit 配置

**获取 API 密钥：**
1. 访问 https://www.reddit.com/prefs/apps
2. 创建 "script" 应用
3. 复制 Client ID 和 Client Secret

**配置环境变量：**
```bash
export REDDIT_CLIENT_ID="your_client_id"
export REDDIT_CLIENT_SECRET="your_client_secret"
export REDDIT_USERNAME="your_username"
export REDDIT_PASSWORD="your_password"
```

### 2. Twitter/X 配置

**获取 API 密钥：**
1. 访问 https://developer.twitter.com/
2. 申请开发者账号
3. 创建 App，获取 API Keys

**配置环境变量：**
```bash
export TWITTER_API_KEY="your_api_key"
export TWITTER_API_SECRET="your_api_secret"
export TWITTER_ACCESS_TOKEN="your_access_token"
export TWITTER_ACCESS_TOKEN_SECRET="your_access_token_secret"
export TWITTER_BEARER_TOKEN="your_bearer_token"
```

### 3. Telegram 配置

**获取 Bot Token：**
1. 在 Telegram 中找到 @BotFather
2. 创建新 bot，复制 Token

**配置环境变量：**
```bash
export TELEGRAM_BOT_TOKEN="your_bot_token"
```

### 4. Discord 配置

**获取 Webhook URL：**
1. 进入 Discord 服务器
2. 频道设置 → Webhooks
3. 创建新 Webhook，复制 URL

**配置环境变量：**
```bash
export DISCORD_WEBHOOK_URL="https://discordapp.com/api/webhooks/..."
```

### 5. 微博配置

**获取 Access Token：**
1. 访问 https://open.weibo.com/
2. 创建应用，获取 App Key 和 Secret

**配置环境变量：**
```bash
export WEIBO_USERNAME="your_username"
export WEIBO_PASSWORD="your_password"
export WEIBO_ACCESS_TOKEN="your_access_token"
```

### 6. 知乎配置

**配置环境变量：**
```bash
export ZHIHU_USERNAME="your_username"
export ZHIHU_PASSWORD="your_password"
```

### 7. 掘金配置

**获取 Token：**
1. 登录掘金后台
2. 获取个人 API Token

**配置环境变量：**
```bash
export JUEJIN_TOKEN="your_token"
```

---

## 自动化工作流

### GitHub Actions 自动发帖

文件 `.github/workflows/auto-posting.yml` 已��置：

- ⏰ **每周一** 上午10点 UTC 自动发帖
- 📅 **每月1号** 上午10点 UTC 自动发帖
- 🏷️ **发布新 tag** 时自动发帖（如 v1.0.0）

**设置 GitHub Secrets：**

1. 进入项目 Settings → Secrets and variables → Actions
2. 添加以下密钥：
   - `REDDIT_CLIENT_ID`
   - `REDDIT_CLIENT_SECRET`
   - `REDDIT_USERNAME`
   - `REDDIT_PASSWORD`
   - `TWITTER_API_KEY`
   - `TWITTER_API_SECRET`
   - 等等...

### 本地定时任务

**Linux/Mac (Crontab)：**
```bash
crontab -e

# 每天上午10点发帖
0 10 * * * cd /path/to/CD12M-kernel-nethunter && python bot_posting.py --all

# 每周一发帖
0 10 * * 1 cd /path/to/CD12M-kernel-nethunter && python bot_posting.py --all

# 每月1号发帖
0 10 1 * * cd /path/to/CD12M-kernel-nethunter && python bot_posting.py --all
```

**Windows (任务计划程序)：**
1. 创建批处理文件 `post_bot.bat`
2. 编辑内容：
```batch
@echo off
cd C:\path\to\CD12M-kernel-nethunter
python bot_posting.py --all
```
3. 通过"任务计划程序"添加定时任务

---

## 发帖内容示例

### Reddit 发帖
```markdown
# 🔓 Kali NetHunter Kernel for CD12M Devices

为 CD12MAX 设备构建的 Kali NetHunter 内核

## 核心功能
✨ HID 键盘攻击模块
🛰️ 外接无线网卡支持
🔌 USB 伪装功能
🔒 SELinux 宽松模式

## 快速开始
```bash
git clone https://github.com/kaliroko/CD12M-kernel-nethunter
make cd12m_defconfig
make -j$(nproc)
```

📥 GitHub: https://github.com/kaliroko/CD12M-kernel-nethunter
```

### Twitter 发帖
```
🔓 Kali NetHunter Kernel for CD12M Now Available!

✨ HID Attacks | 🛰️ WiFi Support | 🔌 USB Spoofing
📦 Linux 4.14.98 | ARM64

🔗 https://github.com/kaliroko/CD12M-kernel-nethunter

#KaliLinux #NetHunter #Security
```

### 微博发帖
```
【Kali NetHunter 内核发布】

为 CD12M 设备适配的 Kali NetHunter 内核已开放！

✨ HID 键盘攻击
🛰️ 外接WiFi支持
🔌 USB伪装
🔒 SELinux宽松

📥 https://github.com/kaliroko/CD12M-kernel-nethunter

#KaliLinux #NetHunter #安全研究
```

---

## 故障排除

### 常见问题

**Q: AttributeError: No module named 'praw'**
```bash
pip install -r requirements.txt
```

**Q: API 认证失败 (401/403)**
- 检查 `.env` 中的密钥是否正确
- 确保 token 未过期
- 重新生成新的 token

**Q: 发帖被标记为垃圾**
- 减少发帖频率（改为周发或月发）
- 使用不同的文案变体
- 加入社区讨论而不仅仅自我推广

**Q: 在 GitHub Actions 中失败**
- 检查 Secrets 是否正确设置
- 查看 Action 日志输出
- 测试本地发帖是否成功

**Q: 如何修改发帖内容？**
- 编辑 `config/` 目录下的对应配置文件
- 修改 `bot_posting.py` 中的 `_get_xxx_body()` 方法
- 重新运行机器人

---

## 日志查看

```bash
# 查看最后发帖日志
tail -f logs/bot_posting.log

# 搜索特定平台的日志
grep "reddit" logs/bot_posting.log
```

---

## 最佳实践

✅ **定期更新内容** - 避免重复相同文案
✅ **社区参与** - 不仅发帖，还要回复讨论
✅ **合规发帖** - 遵守各平台的社群准则
✅ **监控反馈** - 收集用户反馈并改进
✅ **测试部署** - 先在测试账号验证

---

## 许可证

本自动化工具仅供非商业用途使用。
