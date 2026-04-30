# 🎵 NarMusic - 纳儿音乐服务器

基于 C++17 的轻量级音乐下载与管理服务，从 B 站视频提取音频，自动匹配多平台歌词，支持封面嵌入和流式播放。

## ✨ 功能特性

- 🔗 **B站音频提取** — 输入视频链接，自动提取并下载音频
- 🎤 **多平台歌词** — 并发搜索酷狗音乐、网易云音乐、QQ音乐、汽水音乐，智能匹配最佳歌词
- 🖼️ **封面与元数据嵌入** — 歌词、封面、艺术家信息直接写入 M4A 文件
- 🎵 **在线播放** — 浏览器端流式播放，实时歌词同步
- 📱 **响应式界面** — Material Design 3 风格，桌面端和移动端自适应
- ⚡ **轻量高效** — 自研 Epoll HTTP 服务器，适合 ARM64 嵌入式设备
- 🌐 **cpolar 内网穿透** — 内置 cpolar 支持，一行命令暴露公网
- 📮 **邮件通知** — 支持邮箱通知公网IP变化

## 🏗️ 项目架构

```
src/
├── domain/          领域层 — 实体和接口定义
├── application/     应用层 — 业务逻辑编排
├── infrastructure/  基础设施层 — 持久化、HTTP、歌词、音频、内网穿透
└── presentation/    表现层 — HTTP 控制器和静态文件处理
```

采用 DDD 分层 + Repository 模式 + 依赖注入，上层不依赖下层实现细节。

## 🚀 快速开始

### 环境要求

- GCC 8+ (支持 C++17 `<filesystem>`)
- CMake 3.16+
- Make

### 编译 (x86_64 Ubuntu)

```bash
# 安装编译工具
sudo apt install -y build-essential cmake
# 加速编译工具（可选）
sudo apt install ccache ninja-build

# 一键编译
./build.sh
```

### 编译 (ARM64 交叉编译)

```bash
# 安装交叉编译工具链
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# 交叉编译
./build.sh -a aarch64
```

### 运行

```bash
# 开发模式
./build-x86/NarMusic

# 查看使用帮助
./build-x86/NarMusic -h
```

启动后访问 `http://localhost:8080`

## 📦 打包分发

```bash
./build.sh -p
```

生成 `NarMusicServer-dist.zip`，包含可执行文件、Web 界面和配置文件。

## ⚙️ 配置

编辑 `config.json`：

```json
{
    "server": { "port": 8080 },
    "download": { "path": "./download/", "max_age": 600 },
    "lyrics": { "default_platform": "酷狗音乐" },
    "logging": { "level": "info" },
    "database": { "path": "./data/narnat.db" }
}
```

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `server.port` | HTTP 服务端口 | `8080` |
| `server.thread_pool_size` | 线程池大小 | `5` |
| `download.path` | 音频文件保存目录 | `./download/` |
| `download.extension` | 音频文件扩展名 | `.m4a` |
| `download.cleanup_interval` | 清理检查间隔(秒) | `600` |
| `download.max_age` | 任务最大存活时间(秒) | `600` |
| `lyrics.default_platform` | 默认歌词平台 | `酷狗音乐` |
| `lyrics.platforms` | 启用的歌词平台列表 | 酷狗/网易云/QQ/汽水音乐 |
| `logging.level` | 日志级别 (debug/info/warn/error) | `info` |
| `logging.file` | 日志文件路径 | `./logs/narnat.log` |
| `database.path` | SQLite 数据库路径 | `./data/narnat.db` |
| `cpolar.enabled` | 启用 cpolar 内网穿透 | `false` |
| `cpolar.authtoken` | cpolar 认证密钥 | 空 |
| `cpolar.subdomain` | 固定子域名（付费版） | 空 |
| `cpolar.region` | cpolar 区域 | `cn` |
| `cpolar.bin_path` | cpolar 可执行文件路径 | `cpolar` |

## 🛠️ 构建脚本选项

```bash
./build.sh              # 默认: 本机架构 Release 编译
./build.sh -a aarch64   # 交叉编译 ARM64
./build.sh -a x86_64    # 交叉编译 x86_64
./build.sh -t Debug     # Debug 构建
./build.sh -p           # 构建并打包
./build.sh --asan       # ASan 编译模式
./build.sh --lto        # 链接时优化
./build.sh -c           # 清理构建目录
./build.sh -h           # 查看所有选项
```

## 📂 项目结构

```
NarMusic/
├── src/                    C++ 源代码
├── include/                第三方头文件
├── lib/
│   ├── aarch64/            ARM64 预编译库
│   └── x86_64/             x86_64 预编译库
├── web/                    前端界面 (HTML/CSS/JS)
├── cmake/                  CMake 模块
├── build.sh                构建脚本
├── toolchain-aarch64.cmake ARM64 交叉编译工具链
├── config.json             运行时配置
└── CMakeLists.txt          CMake 构建配置
```

## 📋 依赖库

| 库 | 用途 |
|---|---|
| cURL | HTTP 请求 |
| OpenSSL | HTTPS / WBI 签名 |
| SQLite3 | 任务状态存储 |
| TagLib | 音频元数据读写 |
| Gumbo-Parser | HTML 解析 |
| zlib | 压缩 |
| libgsasl | SASL 认证 |
| nlohmann/json | JSON 处理 (header-only) |
| BS_thread_pool | 线程池 (header-only) |
| cpolar | 内网穿透 (外部命令，需单独安装) |

## 🌐 cpolar 内网穿透

NarMusic 内置 cpolar 内网穿透支持，可将本地服务暴露到公网。

### 前置条件

```bash
# 安装 cpolar (方式一：自动安装)
curl -L https://www.cpolar.com/static/downloads/install-release-cpolar.sh | sudo bash

# 方式二：使用打包分发版（cpolar 已内嵌，无需安装）
# 执行 ./build.sh -p 打包时自动下载 cpolar 二进制到分发包
```

## 🔑 cpolar 认证密钥获取

cpolar 内网穿透服务需要认证密钥才能使用，获取步骤如下：

### 1. 注册账号

访问 [cpolar 官网](https://www.cpolar.com/)，点击右上角「注册」按钮，使用邮箱或手机号完成注册。

### 2. 获取 Authtoken

登录后进入「控制台」→「认证」页面，即可看到 `authtoken`：

```
authtoken: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

点击右侧「复制」按钮，将密钥保存备用。

## 🚀 使用

### 方式一：命令行参数（推荐）

```bash
# 启动时直接传入 authtoken，自动启用穿透
./NarMusic -t 你的cpolar_authtoken
```

### 方式二：配置文件

编辑 `config.json`：

```json
{
    "cpolar": {
        "enabled": true,
        "authtoken": "你的authtoken",
        "subdomain": "my-music",
        "region": "cn"
    }
}
```

启动后会在终端显示公网访问地址。


## 📧 QQ 邮箱授权码获取

NarMusic 支持通过 QQ 邮箱发送通知邮件（如下载完成提醒），需使用「授权码」替代邮箱密码登录。

### 1. 开启 SMTP 服务

1. 登录 [QQ 邮箱网页版](https://mail.qq.com/)
2. 点击顶部「设置」→「账户与安全」→「安全设置」
3. 找到「POP3/IMAP/SMTP/Exchange/CardDAV/CalDAV服务」
4. 开启「IMAP/SMTP服务」，按提示发送短信验证

### 2. 获取授权码

验证通过后，系统会生成 16 位授权码：

```
授权码: yyyyyyyyyyyyyyyy
```

## 🚀 使用

### 方式一：命令行参数

```bash
# 启动时直接传入 authtoken，自动启用穿透
./NarMusic -m xxxx@qq.com:yyyyyyyyyyyyyyy
```
### 方式二：配置文件
编辑 `config.json`：

```json
    "email": {
        "enabled": true,
        "smtp_host": "smtp.qq.com",
        "smtp_port": 465,
        "accounts": [
            {
                "sender": "xxx@qq.com",
                "password": "sender1授权码",
                "receiver": "接收方1邮箱"
            }
        ]
    }
```

## 👇 最佳启动命令
```bash
# 启动服务器、启动内网穿透、启动邮件通知
./NarMusic -t xxxxxx -m 123456@qq.com:abcdefghijkl
```


## 其他

### 1.共享文件夹挂载
```bash
## 1.安装工具
sudo apt install open-vm-tools -y

## 2. 查看可用的共享文件夹名称
vmware-hgfsclient

## 3. 创建本地挂载点（如果还没有）
sudo mkdir -p /mnt/hgfs/tanran

## 4.查看可挂载共享文件夹 
vmware-hgfsclient

## 5.将windows下的文件挂载到ubantu
vmhgfs-fuse .host:/tanran /mnt/hgfs/tanran/ -o allow_other,uid=$(id -u),gid=$(id -g)
```

### 2.解决目标机libc库版本不匹配问题，在旧版 glibc 系统上运行需要新版 glibc 的程序
```bash
## 问题
系统 glibc 2.31，程序需要 GLIBC_2.33，无法启动。

## 解决
编译新版 glibc 至独立目录，用 `patchelf` 修改程序动态链接。

## 1. 编译安装 glibc 2.33
sudo apt install -y build-essential bison gawk python3 texinfo patchelf
wget http://ftp.gnu.org/gnu/glibc/glibc-2.33.tar.gz
tar xf glibc-2.33.tar.gz
cd glibc-2.33 && mkdir build && cd build
../configure --prefix=/opt/glibc-2.33
make -j$(nproc)
sudo make install
cd ../..

## 2. 修改程序动态链接
sudo patchelf --set-interpreter /opt/glibc-2.33/lib/ld-linux-aarch64.so.1 ./NarMusic && sudo patchelf --set-rpath '/opt/glibc-2.33/lib:/usr/lib/aarch64-linux-gnu' ./NarMusic 

## 3. 运行
./NarMusic
```

### 3.在86_64架构windows上为aarch64 linux编译

```bash
## 1.配置交叉编译环境
echo 'export CMAKE_GENERATOR="Unix Makefiles"' >> ~/.bashrc
source ~/.bashrc

## 2. 编译 aarch64
./build.sh -a aarch64            --cc aarch64-none-linux-gnu-gcc            --cxx aarch64-none-linux-gnu-g++            --sysroot /d/wtoa/toolchain/aarch64-none-linux-gnu/libc            -j $(nproc)
```

### 4.x86_64架构ubuntu上为aarch64 linux编译

```bash
## 编译并打包
./build.sh --cc /opt/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-rockchip1031-linux-gnu-gcc  --cxx /opt/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-rockchip1031-linux-gnu-g++ -a aarch64 -p
```

### 5.关于编译加速
```bash
./build.sh                    # 全部加速开启 (ccache + Ninja + PCH + Unity)
./build.sh --no-ccache        # 关闭编译缓存，普通编译
./build.sh --no-ninja         # 回退 Make 生成器
./build.sh --no-pch           # 关闭预编译头
./build.sh --no-unity         # 关闭 Unity Build
./build.sh --no-ccache --no-ninja --no-pch --no-unity  # 全部关闭，纯普通编译
```