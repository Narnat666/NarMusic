# 🎵 NarMusic - 纳儿音乐服务器

基于 C++17 的轻量级音乐下载与管理服务，从 B 站视频提取音频，自动匹配多平台歌词，支持封面嵌入和流式播放。

## ✨ 功能特性

- 🔗 **B站音频提取** — 输入视频链接，自动提取并下载音频
- 🎤 **多平台歌词** — 并发搜索酷狗音乐、网易云音乐、QQ音乐，智能匹配最佳歌词
- 🖼️ **封面与元数据嵌入** — 歌词、封面、艺术家信息直接写入 M4A 文件
- 🎵 **在线播放** — 浏览器端流式播放，实时歌词同步
- 📱 **响应式界面** — Material Design 3 风格，桌面端和移动端自适应
- ⚡ **轻量高效** — 自研 Epoll HTTP 服务器，适合 ARM64 嵌入式设备

## 🏗️ 项目架构

```
src/
├── domain/          领域层 — 实体和接口定义
├── application/     应用层 — 业务逻辑编排
├── infrastructure/  基础设施层 — 持久化、HTTP、歌词、音频
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
sudo apt install ccache ninja-build # 加速编译工具

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

# 查看版本
./build-x86/NarMusic --version

# 指定配置文件
./build-x86/NarMusic -c /path/to/config.json

# Debug 模式
./build-x86/NarMusic -d
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

## 🛠️ 构建脚本选项

```bash
./build.sh              # 默认: 本机架构 Release 编译
./build.sh -a aarch64   # 交叉编译 ARM64
./build.sh -a x86_64    # 交叉编译 x86_64
./build.sh -t Debug     # Debug 构建
./build.sh -p           # 构建并打包
./build.sh --asan       # AddressSanitizer
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


## 其他

### 1.共享文件夹挂载
```bash
1.查看可挂载共享文件夹 
vmware-hgfsclient
2.将windows下的文件挂载到ubantu
vmhgfs-fuse .host:/tanran /mnt/hgfs/tanran/ -o allow_other,uid=$(id -u),gid=$(id -g)
```

### 2.解决目标机libc库版本不匹配问题，在旧版 glibc 系统上运行需要新版 glibc 的程序
```bash
## 问题
系统 glibc 2.31，程序需要 GLIBC_2.33，无法启动。

## 解决
编译新版 glibc 至独立目录，用 `patchelf` 修改程序动态链接。

### 1. 编译安装 glibc 2.33
sudo apt install -y build-essential bison gawk python3 texinfo patchelf
wget http://ftp.gnu.org/gnu/glibc/glibc-2.33.tar.gz
tar xf glibc-2.33.tar.gz
cd glibc-2.33 && mkdir build && cd build
../configure --prefix=/opt/glibc-2.33
make -j$(nproc)
sudo make install
cd ../..

### 2. 修改程序动态链接

sudo patchelf --set-interpreter /opt/glibc-2.33/lib/ld-linux-aarch64.so.1 ./NarMusic && sudo patchelf --set-rpath '/opt/glibc-2.33/lib:/usr/lib/aarch64-linux-gnu' ./NarMusic 

### 3. 运行
./NarMusic
```

### 3.在86_64架构windows上为aarch64 linux编译

```bash
echo 'export CMAKE_GENERATOR="Unix Makefiles"' >> ~/.bashrc
source ~/.bashrc

./build.sh -a aarch64            --cc aarch64-none-linux-gnu-gcc            --cxx aarch64-none-linux-gnu-g++            --sysroot /d/wtoa/toolchain/aarch64-none-linux-gnu/libc            -j $(nproc)
```

### 4.x86_64架构ubuntu上为aarch64 linux编译

```bash
./build.sh --cc /opt/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-rockchip1031-linux-gnu-gcc  --cxx /opt/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-rockchip1031-linux-gnu-g++ -a aarch64 -p # 配置自己的交叉工具链
```

### 5.普通编译
```bash
./build.sh                    # 全部加速开启 (ccache + Ninja + PCH + Unity)
./build.sh --no-ccache        # 关闭编译缓存，普通编译
./build.sh --no-ninja         # 回退 Make 生成器
./build.sh --no-pch           # 关闭预编译头
./build.sh --no-unity         # 关闭 Unity Build
./build.sh --no-ccache --no-ninja --no-pch --no-unity  # 全部关闭，纯普通编译
```