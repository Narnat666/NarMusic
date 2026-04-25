# NarMusic

交叉编译：
mkdir build-aarch64
cd build-aarch64
cmake -D CMAKE_TOOLCHAIN_FILE=../toolchain-aarch64.cmake ..
make

# 学习目标和成果

1. cmake的基础语法了解以及基础的项目构建 [完成]
2. 交叉编译器配合cmake编译c++项目 [完成]
3. 了解脚本基础语法和使用脚本 [完成]
4. 巩固c++知识 [正在进行]
5. 线程池管理线程 [完成]
6. 优化网络协议代码、利用buildroot编译可能的目标架构依赖库 [大部分完成]
7. 交叉编译curl库利用curl库函数和nlohmann开源库优化项目代码，链接依赖库 [完成]
8. 深刻了解并解决交叉编译器版本不匹配导致的许多链接问题 [完成]
9. 静态库交叉编译项目 [完成]
10. 音频流媒体技术了解和学习，将音频像流水一样发送给客户端 [待完成]
11. 代码整合，更进一步理解c++面向对象编程核心 [待完成]
12. 移值github音乐文件写入库 [完成]
13. 音乐文件支持歌词写入、封面写入、歌词与音频对齐功能 [完成]
14. 通过脚本控制程序，守护进程 [完成]

# 后续新增功能
后续有时间想试着添加下述功能:
1. 将后端适配到入选网页中的椒盐音乐电脑版网页和椒盐音乐移动端版网页 [完成]
2. 页面中有音乐库模块，可以用来查看所有近期已下载音乐文件，下载音乐到本地功能计划迁移到这里 [完成]
3. 页面中添加了设置按钮计划将音乐平台、延迟、文件删除时间在这里配置 [部分完成] （文件删除时间期望由系统配置）
4. 音乐播放功能可以播放音乐库中的文件 [完成]
5. 计划重构后端代码 [未完成]
6. 页面布局还要重构，但主题风格决定不再调整 [部分完成]
上述功能计划用TRAE工具实现，让ai写代码我来，审核、调试、代码
目前没有时间，这个是以后的计划 [搞得很快，比我想象的要快借助AI开发还是比较方便的]

# 待优化
搜索模块：计划通过歌曲名-作者提取音乐链接 回显到音乐链接模块，回显到自定义文件名模块 [待完成]
音乐弹窗：音乐弹窗模块计划添加详情点击功能，点击后能正常查看歌词和音乐播放，方便调试 [待完成]
移动端和电脑端计划分开显示，用两个不同的界面代码 [废弃，直接用一个界面做移动和电脑端的适配即可]
最开始打开网页会布局会乱，这个需要修复 [待完成]

# 关于重构

1. 后端计划采用企业级重构方案，配合AI开发，我来审核，计划代码全部由AI编写 [待完成]
    1、计划添加详细的日志记录模块
    2、现有架构可能不够完美，计划优化架构
    3、计划设计成一个专业的开源项目，完成后上传到github
    4、计划将项目适配嵌入式和windows平台，通过脚本灵活编译

...

# 共享文件夹挂载
1.查看可挂载共享文件夹 
vmware-hgfsclient
2.将windows下的文件挂载到ubantu
vmhgfs-fuse .host:/tanran /mnt/hgfs/tanran/ -o allow_other,uid=$(id -u),gid=$(id -g)

# 程序完全静默运行
(./NarMusic -p ./download/ -e .m4a > NarMusic.log 2>&1 &)

# 解决目标机libc库版本不匹配问题，在旧版 glibc 系统上运行需要新版 glibc 的程序

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

sudo patchelf --set-interpreter /opt/glibc-2.33/lib/ld-linux-aarch64.so.1 ./NarMusic
sudo patchelf --set-rpath '/opt/glibc-2.33/lib:/usr/lib/aarch64-linux-gnu' ./NarMusic 

### 3. 运行
./NarMusic

# 构建说明

## 构建脚本

项目提供了一个专业的构建脚本 `build.sh`，专门为ARM64 Linux平台设计，支持交叉编译和本地编译。

### 快速开始

```bash
# 查看帮助信息
./build.sh --help

# 默认构建 (ARM64交叉编译)
./build.sh

# Debug构建
./build.sh -t Debug

# 本地构建 (在ARM64设备上)
./build.sh -a native

# 清理构建目录
./build.sh -c

# 创建发布包
./build.sh -p

# 构建并安装到系统
./build.sh -i

# 交叉编译项目
./build.sh --cc /opt/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-rockchip1031-linux-gnu-gcc  --cxx /opt/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-rockchip1031-linux-gnu-g++ -a aarch64

```

### 自定义交叉编译工具链

```bash
# 指定编译器路径
./build.sh --cc /path/to/aarch64-linux-gnu-gcc --cxx /path/to/aarch64-linux-gnu-g++

# 使用环境变量
export CC=/path/to/aarch64-linux-gnu-gcc
export CXX=/path/to/aarch64-linux-gnu-g++
./build.sh

# 指定sysroot
./build.sh --sysroot /path/to/sysroot

# 使用工具链文件
./build.sh --toolchain toolchain-aarch64.cmake
```

### 构建配置

编辑 `build.config` 文件可以自定义构建配置：

```bash
# 项目信息
PROJECT_NAME="NarMusic"
PROJECT_VERSION="1.0.0"

# 默认构建配置
DEFAULT_BUILD_TYPE="Release"
DEFAULT_ARCH="aarch64"
DEFAULT_PLATFORM="linux"

# 自定义编译器路径
# CC="/opt/linaro-12.3/bin/aarch64-linux-gnu-gcc"
# CXX="/opt/linaro-12.3/bin/aarch64-linux-gnu-g++"
# SYSROOT_DIR="/opt/linaro-12.3/aarch64-linux-gnu/sysroot"
```

### 依赖库

项目包含预编译的ARM64依赖库：
- 库文件：`lib/` 目录
- 头文件：`include/` 目录

包含以下库：
- libcurl - HTTP客户端库
- OpenSSL - 加密库
- TagLib - 音频元数据库
- Gumbo - HTML解析库

### 发布包

构建完成后可以创建发布包：
```bash
./build.sh -p
```

发布包位于 `dist/` 目录，包含：
- 可执行文件
- 启动脚本
- 服务文件
- Web界面文件
- README和许可证

### 在ARM64设备上运行

1. 传输文件到ARM64设备：
   ```bash
   scp dist/NarMusic-1.0.0-aarch64-linux.tar.gz user@arm64-device:/tmp/
   ```

2. 在ARM64设备上运行：
   ```bash
   tar -xzf NarMusic-1.0.0-aarch64-linux.tar.gz
   cd NarMusic-1.0.0-aarch64-linux
   chmod +x NarMusic start.sh
   ./start.sh
   ```

### 系统服务安装

```bash
# 复制服务文件
sudo cp service/NarMusic.service /etc/systemd/system/

# 编辑服务文件中的路径
sudo nano /etc/systemd/system/NarMusic.service

# 启用并启动服务
sudo systemctl daemon-reload
sudo systemctl enable NarMusic
sudo systemctl start NarMusic
```

### 故障排除

#### 交叉编译工具链未找到
```bash
# 安装交叉编译工具链 (Ubuntu/Debian)
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu cmake make
```

#### 权限问题
```bash
# 安装时需要sudo权限
sudo ./build.sh -i

# 或使用用户目录
./build.sh -i --prefix ~/.local
```

#### 详细输出
```bash
# 启用详细输出查看问题
./build.sh -v

# 干运行模式（只显示将要执行的操作）
./build.sh -n
```

## 手动构建

如果需要手动构建，可以使用以下命令：

```bash
# ARM64交叉编译
mkdir build-aarch64
cd build-aarch64
cmake -D CMAKE_TOOLCHAIN_FILE=../toolchain-aarch64.cmake ..
make -j$(nproc)

# 本地构建 (在ARM64设备上)
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## 项目结构

```
NarMusic/
├── build.sh              # 专业构建脚本
├── build.config          # 构建配置文件
├── toolchain-aarch64.cmake # ARM64交叉编译工具链
├── CMakeLists.txt        # CMake构建配置
├── src/                  # 源代码
├── include/              # 头文件
├── lib/                  # 预编译的ARM64库
├── service/              # 服务管理文件
├── web/                  # Web界面
├── dist/                 # 发布包目录
└── README.md            # 项目说明
```