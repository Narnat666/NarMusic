#!/usr/bin/env bash

# ============================================================================
# C++ HTTP Server ARM64 Linux 构建脚本
# 专门为ARM64 Linux平台设计，支持交叉编译
# ============================================================================

set -euo pipefail

# ============================================================================
# 配置部分
# ============================================================================

# 项目信息
PROJECT_NAME="NarMusic"
PROJECT_VERSION="1.0.0"
PROJECT_DESCRIPTION="C++ Music Server for ARM64 Linux"

# 默认构建配置
DEFAULT_BUILD_TYPE="Release"
DEFAULT_ARCH="aarch64"           # 专注于ARM64
DEFAULT_PLATFORM="linux"         # 专注于Linux

# 交叉编译工具链配置
# 默认使用系统PATH中的交叉编译器
# 可以通过--cc/--cxx参数或CC/CXX环境变量覆盖
TOOLCHAIN_PREFIX="aarch64-linux-gnu"
DEFAULT_CC="${TOOLCHAIN_PREFIX}-gcc"
DEFAULT_CXX="${TOOLCHAIN_PREFIX}-g++"

# 输出目录配置
BUILD_DIR_PREFIX="build"
INSTALL_PREFIX="/usr/local"
PACKAGE_DIR="dist"

# ============================================================================
# 颜色定义
# ============================================================================

if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    MAGENTA='\033[0;35m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    MAGENTA=''
    CYAN=''
    BOLD=''
    NC=''
fi

# ============================================================================
# 日志函数
# ============================================================================

log() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

debug() {
    if [[ "${VERBOSE:-false}" == "true" ]]; then
        echo -e "${CYAN}[DEBUG]${NC} $1"
    fi
}

# ============================================================================
# 工具函数
# ============================================================================

# 检查命令是否存在
check_command() {
    if ! command -v "$1" &> /dev/null; then
        error "命令 '$1' 未找到，请安装后重试"
        return 1
    fi
}

# 获取CPU核心数
get_cpu_cores() {
    if command -v nproc &> /dev/null; then
        nproc
    elif [[ "$(uname)" == "Darwin" ]]; then
        sysctl -n hw.ncpu
    else
        echo 4
    fi
}

# 检测主机架构
detect_host_arch() {
    case "$(uname -m)" in
        x86_64|amd64)   echo "x86_64" ;;
        aarch64|arm64)  echo "aarch64" ;;
        armv7l|armhf)   echo "armv7" ;;
        i386|i686)      echo "x86" ;;
        *)              echo "unknown" ;;
    esac
}

# 检测操作系统
detect_os() {
    case "$(uname -s)" in
        Linux*)     echo "linux" ;;
        Darwin*)    echo "macos" ;;
        CYGWIN*|MINGW*|MSYS*) echo "windows" ;;
        *)          echo "unknown" ;;
    esac
}

# ============================================================================
# 参数解析
# ============================================================================

parse_args() {
    BUILD_TYPE="${DEFAULT_BUILD_TYPE}"
    ARCH="${DEFAULT_ARCH}"
    PLATFORM="${DEFAULT_PLATFORM}"
    CLEAN=false
    VERBOSE=false
    INSTALL=false
    PACKAGE=false
    DRY_RUN=false
    CMAKE_EXTRA_ARGS=()
    
    # 编译器设置
    CC="${DEFAULT_CC}"
    CXX="${DEFAULT_CXX}"
    SYSROOT_DIR=""
    TOOLCHAIN_FILE=""
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --type|-t)
                BUILD_TYPE="$2"
                shift 2
                ;;
            --arch|-a)
                ARCH="$2"
                shift 2
                ;;
            --clean|-c)
                CLEAN=true
                shift
                ;;
            --verbose|-v)
                VERBOSE=true
                shift
                ;;
            --install|-i)
                INSTALL=true
                shift
                ;;
            --package|-p)
                PACKAGE=true
                shift
                ;;
            --dry-run|-n)
                DRY_RUN=true
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            --version|-V)
                show_version
                exit 0
                ;;
            --toolchain)
                TOOLCHAIN_FILE="$2"
                shift 2
                ;;
            --prefix)
                INSTALL_PREFIX="$2"
                shift 2
                ;;
            --jobs|-j)
                JOBS="$2"
                shift 2
                ;;
            --cc)
                CC="$2"
                shift 2
                ;;
            --cxx)
                CXX="$2"
                shift 2
                ;;
            --sysroot)
                SYSROOT_DIR="$2"
                shift 2
                ;;
            --*|-*)
                # 传递给CMake的额外参数
                CMAKE_EXTRA_ARGS+=("$1")
                if [[ $# -gt 1 && "$2" != --* && "$2" != -* ]]; then
                    CMAKE_EXTRA_ARGS+=("$2")
                    shift 2
                else
                    shift
                fi
                ;;
            *)
                error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # 设置默认值
    JOBS="${JOBS:-$(get_cpu_cores)}"
    
    # 环境变量覆盖（如果设置了环境变量，则覆盖命令行参数）
    if [[ -n "${CC_ENV:-}" ]]; then
        CC="${CC_ENV}"
        log "使用环境变量 CC: ${CC}"
    fi
    
    if [[ -n "${CXX_ENV:-}" ]]; then
        CXX="${CXX_ENV}"
        log "使用环境变量 CXX: ${CXX}"
    fi
    
    # 构建目录名称
    if [[ "${ARCH}" == "native" ]]; then
        ARCH="$(detect_host_arch)"
    fi
    
    BUILD_DIR="${BUILD_DIR_PREFIX}-${ARCH}-${BUILD_TYPE}"
}

# ============================================================================
# 显示函数
# ============================================================================

show_version() {
    echo "${PROJECT_NAME} v${PROJECT_VERSION}"
    echo "${PROJECT_DESCRIPTION}"
    echo "目标平台: ${ARCH} Linux"
}

show_help() {
    cat << EOF
${PROJECT_NAME} ARM64 Linux 构建脚本 v${PROJECT_VERSION}

专门为ARM64 Linux平台设计的构建脚本，支持交叉编译和本地编译

用法: $0 [选项]

选项:
  构建选项:
    -t, --type TYPE        构建类型 (Release, Debug, RelWithDebInfo, MinSizeRel)
                           [默认: ${DEFAULT_BUILD_TYPE}]
    -a, --arch ARCH        目标架构 (native, aarch64)
                           [默认: ${DEFAULT_ARCH}]
                           native: 自动检测当前架构
                           aarch64: ARM64交叉编译
    --toolchain FILE       指定CMake工具链文件
    -j, --jobs N           并行编译任务数 [默认: CPU核心数]
    --cc COMPILER          指定C编译器 [默认: ${DEFAULT_CC}]
    --cxx COMPILER         指定C++编译器 [默认: ${DEFAULT_CXX}]
    --sysroot DIR          指定sysroot目录

  操作选项:
    -c, --clean            清理构建目录
    -i, --install          安装到系统目录
    -p, --package          创建发布包
    -n, --dry-run          只显示将要执行的操作，不实际执行

  信息选项:
    -h, --help             显示此帮助信息
    -V, --version          显示版本信息

  其他选项:
    -v, --verbose          显示详细输出
    --prefix DIR           安装目录前缀 [默认: ${INSTALL_PREFIX}]
    --                     传递额外参数给CMake

示例:
  $0                         # 默认构建 (Release, ARM64交叉编译)
  $0 -t Debug                # Debug构建
  $0 -a native               # 本地构建（自动检测架构）
  $0 -c                      # 清理构建目录
  $0 -i                      # 构建并安装
  $0 -p                      # 创建发布包
  $0 --cc /path/to/aarch64-linux-gnu-gcc --cxx /path/to/aarch64-linux-gnu-g++  # 指定编译器

环境变量:
  CC                        C编译器 (覆盖--cc选项)
  CXX                       C++编译器 (覆盖--cxx选项)
  SYSROOT_DIR              sysroot目录

交叉编译工具链:
  推荐使用以下工具链之一:
  - GCC Linaro: https://releases.linaro.org/components/toolchain/binaries/
  - ARM官方工具链: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain

  安装示例 (Ubuntu):
    sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu cmake make

本地编译:
  在ARM64设备上直接运行: $0 -a native
EOF
}

# ============================================================================
# 检查要求
# ============================================================================

check_requirements() {
    log "检查构建要求..."
    
    local missing_tools=()
    
    # 检查基本工具
    for tool in cmake make; do
        if ! check_command "$tool"; then
            missing_tools+=("$tool")
        fi
    done
    
    # 检查编译器
    if ! check_command "${CC}"; then
        warning "编译器未找到: ${CC}"
        
        # 如果是交叉编译器，提供安装指南
        if [[ "${ARCH}" == "aarch64" && "${CC}" == "${DEFAULT_CC}" ]]; then
            log "尝试在PATH中查找其他交叉编译器..."
            if command -v "aarch64-linux-gnu-gcc" &> /dev/null; then
                CC="aarch64-linux-gnu-gcc"
                CXX="aarch64-linux-gnu-g++"
                log "找到交叉编译器: CC=${CC}, CXX=${CXX}"
            else
                missing_tools+=("${CC} (交叉编译器)")
                echo ""
                echo "安装交叉编译工具链 (Ubuntu/Debian):"
                echo "  sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
            fi
        elif [[ "${ARCH}" == "native" ]]; then
            # 本地编译，使用系统编译器
            if command -v "gcc" &> /dev/null; then
                CC="gcc"
                CXX="g++"
                log "使用系统编译器: CC=${CC}, CXX=${CXX}"
            else
                missing_tools+=("gcc (C编译器)")
            fi
        else
            missing_tools+=("${CC} (编译器)")
        fi
    fi
    
    # 检查C++编译器
    if ! check_command "${CXX}"; then
        if [[ "${ARCH}" == "native" && "${CXX}" == "${DEFAULT_CXX}" ]]; then
            if command -v "g++" &> /dev/null; then
                CXX="g++"
                log "使用系统C++编译器: ${CXX}"
            else
                missing_tools+=("g++ (C++编译器)")
            fi
        else
            missing_tools+=("${CXX} (C++编译器)")
        fi
    fi
    
    # 检查主机架构
    local host_arch=$(detect_host_arch)
    local host_os=$(detect_os)
    
    if [[ "${ARCH}" == "aarch64" && "${host_arch}" == "aarch64" ]]; then
        log "在ARM64设备上进行本地编译"
        ARCH="native"
        CC="gcc"
        CXX="g++"
    elif [[ "${ARCH}" == "aarch64" && "${host_arch}" != "x86_64" ]]; then
        warning "非x86_64主机进行ARM64交叉编译: ${host_arch}"
        warning "交叉编译可能无法正常工作"
    fi
    
    if [[ "${host_os}" != "linux" ]]; then
        warning "非Linux主机: ${host_os}"
        warning "建议在Linux环境下构建"
    fi
    
    if [[ ${#missing_tools[@]} -ne 0 ]]; then
        error "缺少必要的构建工具:"
        for tool in "${missing_tools[@]}"; do
            echo "  - $tool"
        done
        exit 1
    fi
    
    success "所有构建要求检查通过"
    
    # 显示构建配置
    log "构建配置:"
    log "  项目: ${PROJECT_NAME} v${PROJECT_VERSION}"
    log "  类型: ${BUILD_TYPE}"
    log "  架构: ${ARCH}"
    log "  主机: ${host_arch} (${host_os})"
    log "  目录: ${BUILD_DIR}"
    log "  任务: ${JOBS} 并行"
    log "  编译器: CC=${CC}, CXX=${CXX}"
    
    if [[ -n "${SYSROOT_DIR}" && -d "${SYSROOT_DIR}" ]]; then
        log "  Sysroot: ${SYSROOT_DIR}"
    fi
    
    if [[ -n "${TOOLCHAIN_FILE}" && -f "${TOOLCHAIN_FILE}" ]]; then
        log "  工具链文件: ${TOOLCHAIN_FILE}"
    fi
    
    if [[ "${VERBOSE}" == "true" ]]; then
        log "  CMake额外参数: ${CMAKE_EXTRA_ARGS[*]}"
    fi
}

# ============================================================================
# 清理函数
# ============================================================================

clean_build() {
    log "清理构建目录..."
    
    local dirs_to_clean=(
        "${BUILD_DIR_PREFIX}"-*
        "CMakeCache.txt"
        "CMakeFiles"
        "cmake_install.cmake"
        "Makefile"
        "*.cmake"
    )
    
    for pattern in "${dirs_to_clean[@]}"; do
        if [[ -e "${pattern}" ]]; then
            if [[ "${DRY_RUN}" == "true" ]]; then
                echo "  [DRY RUN] 删除: ${pattern}"
            else
                rm -rf ${pattern}
            fi
        fi
    done
    
    success "构建目录已清理"
}

# ============================================================================
# 配置CMake
# ============================================================================

configure_cmake() {
    log "配置CMake..."
    
    # 创建构建目录
    if [[ ! -d "${BUILD_DIR}" ]]; then
        if [[ "${DRY_RUN}" == "true" ]]; then
            echo "  [DRY RUN] 创建目录: ${BUILD_DIR}"
        else
            mkdir -p "${BUILD_DIR}"
            success "创建构建目录: ${BUILD_DIR}"
        fi
    fi
    
    # 进入构建目录
    cd "${BUILD_DIR}"
    
    # 准备CMake参数
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
        "-DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    )
    
    # 设置编译器
    cmake_args+=("-DCMAKE_C_COMPILER=${CC}")
    cmake_args+=("-DCMAKE_CXX_COMPILER=${CXX}")
    
    # 交叉编译设置
    if [[ "${ARCH}" == "aarch64" ]]; then
        cmake_args+=("-DCMAKE_SYSTEM_NAME=Linux")
        cmake_args+=("-DCMAKE_SYSTEM_PROCESSOR=aarch64")
        cmake_args+=("-DCMAKE_C_FLAGS=-march=armv8-a")
        cmake_args+=("-DCMAKE_CXX_FLAGS=-march=armv8-a")
        
        # 设置sysroot（如果存在）
        if [[ -n "${SYSROOT_DIR}" && -d "${SYSROOT_DIR}" ]]; then
            cmake_args+=("-DCMAKE_SYSROOT=${SYSROOT_DIR}")
            cmake_args+=("-DCMAKE_FIND_ROOT_PATH=${SYSROOT_DIR}")
            cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER")
            cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY")
            cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY")
            cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY")
        fi
        
        # 使用工具链文件（如果指定）
        if [[ -n "${TOOLCHAIN_FILE}" && -f "${TOOLCHAIN_FILE}" ]]; then
            cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
            log "使用工具链文件: ${TOOLCHAIN_FILE}"
        fi
    fi
    
    # 添加额外参数
    cmake_args+=("${CMAKE_EXTRA_ARGS[@]}")
    cmake_args+=("..")
    
    # 执行CMake配置
    if [[ "${DRY_RUN}" == "true" ]]; then
        echo "  [DRY RUN] cmake ${cmake_args[*]}"
    else
        debug "CMake命令: cmake ${cmake_args[*]}"
        
        log "运行CMake配置..."
        if cmake "${cmake_args[@]}"; then
            success "CMake配置成功"
            
            # 显示编译器信息
            if [[ "${VERBOSE}" == "true" ]]; then
                echo ""
                log "编译器信息:"
                "${CC}" --version | head -1
                "${CXX}" --version | head -1
            fi
        else
            error "CMake配置失败"
            cd ..
            exit 1
        fi
    fi
    
    # 返回项目根目录
    cd ..
}

# ============================================================================
# 编译项目
# ============================================================================

build_project() {
    log "编译项目..."
    
    if [[ ! -d "${BUILD_DIR}" ]]; then
        error "构建目录不存在: ${BUILD_DIR}"
        exit 1
    fi
    
    cd "${BUILD_DIR}"
    
    if [[ "${DRY_RUN}" == "true" ]]; then
        echo "  [DRY RUN] make -j${JOBS}"
    else
        debug "编译命令: make -j${JOBS}"
        
        log "开始编译..."
        if make -j"${JOBS}"; then
            success "编译成功完成！"
            
            # 显示生成的可执行文件信息
            if [[ -f "${PROJECT_NAME}" ]]; then
                echo ""
                log "生成的可执行文件:"
                ls -lh "${PROJECT_NAME}"
                
                # 检查文件类型
                log "文件类型:"
                file "${PROJECT_NAME}"
                
                # 检查架构
                log "目标架构: ${ARCH}"
            fi
        else
            error "编译失败"
            cd ..
            exit 1
        fi
    fi
    
    cd ..
}

# ============================================================================
# 创建发布包
# ============================================================================

create_package() {
    log "创建发布包..."
    
    if [[ ! -d "${BUILD_DIR}" ]]; then
        error "构建目录不存在: ${BUILD_DIR}"
        exit 1
    fi
    
    if [[ ! -f "${BUILD_DIR}/${PROJECT_NAME}" ]]; then
        error "可执行文件未找到: ${BUILD_DIR}/${PROJECT_NAME}"
        exit 1
    fi
    
    # 创建包目录
    local package_name="${PROJECT_NAME}-${PROJECT_VERSION}-${ARCH}-linux"
    local package_dir="${PACKAGE_DIR}/${package_name}"
    
    if [[ "${DRY_RUN}" == "true" ]]; then
        echo "  [DRY RUN] 创建包目录: ${package_dir}"
        echo "  [DRY RUN] 复制文件到包目录"
        echo "  [DRY RUN] 创建压缩包: ${package_name}.tar.gz"
    else
        mkdir -p "${package_dir}"
        
        # 复制可执行文件
        log "复制可执行文件..."
        cp "${BUILD_DIR}/${PROJECT_NAME}" "${package_dir}/"
        
        # 复制服务文件
        if [[ -d "service" ]]; then
            log "复制服务文件..."
            cp -r "service" "${package_dir}/"
        fi
        
        # 复制web文件
        if [[ -d "web" ]]; then
            log "复制Web文件..."
            cp -r "web" "${package_dir}/"
        fi
        
        # 复制README和许可证
        for file in README.md LICENSE*; do
            if [[ -f "${file}" ]]; then
                cp "${file}" "${package_dir}/"
            fi
        done
        
        # 创建启动脚本
        cat > "${package_dir}/start.sh" << 'EOF'
#!/bin/bash

# NarMusic 启动脚本

set -e

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

# 检查可执行文件
if [[ ! -f "./NarMusic" ]]; then
    echo "错误: 未找到 NarMusic 可执行文件"
    exit 1
fi

# 检查架构
ARCH=$(uname -m)
echo "系统架构: ${ARCH}"

# 设置默认参数
PORT=8080
DOWNLOAD_DIR="./download"
EXTENSIONS=".m4a,.mp3,.flac,.wav"
LOG_FILE="NarMusic.log"

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -d|--download-dir)
            DOWNLOAD_DIR="$2"
            shift 2
            ;;
        -e|--extensions)
            EXTENSIONS="$2"
            shift 2
            ;;
        -l|--log)
            LOG_FILE="$2"
            shift 2
            ;;
        -h|--help)
            echo "用法: $0 [选项]"
            echo "选项:"
            echo "  -p, --port PORT        监听端口 [默认: 8080]"
            echo "  -d, --download-dir DIR 下载目录 [默认: ./download]"
            echo "  -e, --extensions EXTS  支持的文件扩展名 [默认: .m4a,.mp3,.flac,.wav]"
            echo "  -l, --log FILE         日志文件 [默认: NarMusic.log]"
            echo "  -h, --help             显示此帮助信息"
            exit 0
            ;;
        *)
            echo "未知参数: $1"
            exit 1
            ;;
    esac
done

# 创建下载目录
mkdir -p "${DOWNLOAD_DIR}"

# 启动服务器
echo "启动 NarMusic..."
echo "端口: ${PORT}"
echo "下载目录: ${DOWNLOAD_DIR}"
echo "支持扩展名: ${EXTENSIONS}"
echo "日志文件: ${LOG_FILE}"

./NarMusic -p "${DOWNLOAD_DIR}" -e "${EXTENSIONS}" > "${LOG_FILE}" 2>&1 &

SERVER_PID=$!
echo "服务器已启动，PID: ${SERVER_PID}"
echo "停止服务器: kill ${SERVER_PID}"
echo "查看日志: tail -f ${LOG_FILE}"
EOF
        
        chmod +x "${package_dir}/start.sh"
        
        # 创建压缩包
        cd "${PACKAGE_DIR}"
        log "创建压缩包..."
        tar -czf "${package_name}.tar.gz" "${package_name}"
        
        success "发布包创建成功: ${PACKAGE_DIR}/${package_name}.tar.gz"
        log "包大小:"
        ls -lh "${package_name}.tar.gz"
        
        cd ..
    fi
}

# ============================================================================
# 主函数
# ============================================================================

main() {
    echo "=========================================="
    echo "  ${PROJECT_NAME} ARM64 Linux 构建脚本"
    echo "  版本: ${PROJECT_VERSION}"
    echo "=========================================="
    echo ""
    
    # 解析参数
    parse_args "$@"
    
    # 显示构建信息
    if [[ "${DRY_RUN}" == "true" ]]; then
        warning "干运行模式 - 只显示将要执行的操作"
        echo ""
    fi
    
    # 清理操作
    if [[ "${CLEAN}" == "true" ]]; then
        clean_build
        if [[ "${CLEAN}" == "true" && "${#}" -eq 1 ]]; then
            # 如果只有clean参数，则退出
            exit 0
        fi
    fi
    
    # 构建流程
    if [[ "${DRY_RUN}" == "false" || "${CLEAN}" == "false" ]]; then
        check_requirements
        configure_cmake
        build_project
        
        # 打包操作
        if [[ "${PACKAGE}" == "true" ]]; then
            create_package
        fi
        
        echo ""
        success "构建完成！"
        
        # 显示下一步建议
        echo ""
        log "下一步建议:"
        log "  运行程序: ./${BUILD_DIR}/${PROJECT_NAME}"
        
        if [[ "${PACKAGE}" == "false" ]]; then
            log "  创建发布包: $0 -p"
        fi
        
        log "  清理构建: $0 -c"
        
        if [[ "${ARCH}" == "aarch64" ]]; then
            log "  复制到ARM64设备: scp ./${BUILD_DIR}/${PROJECT_NAME} user@arm64-device:/path/to/"
        fi
        
        # 显示架构信息
        echo ""
        log "架构验证:"
        if command -v file &> /dev/null && [[ -f "${BUILD_DIR}/${PROJECT_NAME}" ]]; then
            file "${BUILD_DIR}/${PROJECT_NAME}"
        fi
    fi
}

# ============================================================================
# 脚本入口
# ============================================================================

# 确保在脚本目录中运行
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

# 运行主函数
main "$@"