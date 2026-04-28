#!/usr/bin/env bash
# ============================================================================
# NarMusic 构建脚本
# 支持: 本地编译 / 交叉编译 (aarch64|x86_64) / 消毒器 / LTO / 打包
# ============================================================================

set -euo pipefail

# ============================================================================
# 常量
# ============================================================================
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_NAME="NarMusic"
readonly PROJECT_VERSION="2.0.0"
readonly DEFAULT_BUILD_TYPE="Release"

# ============================================================================
# 颜色
# ============================================================================
if [[ -t 1 ]]; then
    readonly R='\033[0;31m' G='\033[0;32m' Y='\033[1;33m' B='\033[0;34m' C='\033[0;36m' N='\033[0m'
else
    readonly R='' G='' Y='' B='' C='' N=''
fi

log()    { echo -e "${B}[INFO]${N} $1"; }
ok()     { echo -e "${G}[OK]${N} $1"; }
warn()   { echo -e "${Y}[WARN]${N} $1"; }
err()    { echo -e "${R}[ERR]${N} $1" >&2; }
die()    { err "$1"; exit 1; }
debug()  { [[ "${VERBOSE:-0}" == 1 ]] && echo -e "${C}[DBG]${N} $1"; }

# ============================================================================
# 工具
# ============================================================================
detect_host_arch() {
    case "$(uname -m)" in
        x86_64|amd64)  echo "x86_64" ;;
        aarch64|arm64) echo "aarch64" ;;
        *)             echo "unknown" ;;
    esac
}

nproc_fallback() {
    if command -v nproc &>/dev/null; then nproc
    elif [[ "$(uname)" == "Darwin" ]]; then sysctl -n hw.ncpu
    else echo 4
    fi
}

# 默认架构 = 本机架构
readonly HOST_ARCH="$(detect_host_arch)"
readonly DEFAULT_ARCH="${HOST_ARCH}"

# ============================================================================
# 参数
# ============================================================================
BUILD_TYPE="${DEFAULT_BUILD_TYPE}"
ARCH="${DEFAULT_ARCH}"
CLEAN=0
VERBOSE=0
INSTALL=0
PACKAGE=0
DRY_RUN=0
JOBS=""
CC="" CXX=""
TOOLCHAIN_FILE="" SYSROOT_DIR=""
INSTALL_PREFIX="/usr/local"
CMAKE_EXTRA=()
OPT_LTO=0 OPT_ASAN=0 OPT_TSAN=0 OPT_UBSAN=0 OPT_WERROR=0 OPT_STATIC=0

usage() {
    cat <<EOF
${PROJECT_NAME} v${PROJECT_VERSION} 构建脚本

用法: $0 [选项]

构建:
  -t, --type TYPE       构建类型 (Release|Debug|RelWithDebInfo|MinSizeRel) [默认: ${DEFAULT_BUILD_TYPE}]
  -a, --arch ARCH       目标架构 (native|x86_64|aarch64) [默认: ${DEFAULT_ARCH} (本机架构)]
  --toolchain FILE      CMake 工具链文件 (交叉编译时自动选择)
  --cc CC               C 编译器
  --cxx CXX             C++ 编译器
  --sysroot DIR         Sysroot 目录
  -j, --jobs N          并行任务数 [默认: CPU 核心数]

选项开关:
  --lto                 启用链接时优化
  --asan                启用 AddressSanitizer
  --tsan                启用 ThreadSanitizer
  --ubsan               启用 UBSanitizer
  --werror              警告视为错误
  --static              完全静态链接 (含 libc)

操作:
  -c, --clean           清理构建目录
  -i, --install         构建并安装
  -p, --package         构建并打包
  -n, --dry-run         仅显示操作不执行

信息:
  -v, --verbose         详细输出
  -h, --help            显示帮助
  -V, --version         显示版本

示例:
  $0                          # 默认: 本机架构 Release 编译
  $0 -a aarch64               # 交叉编译 ARM64
  $0 -a x86_64                # 交叉编译 x86_64 (在 ARM 主机上)
  $0 -t Debug -a native       # 本机 Debug 构建
  $0 --asan -t Debug          # Debug + ASan
  $0 --lto -t Release         # Release + LTO
  $0 -p                       # 构建并打包
EOF
}

show_version() { echo "${PROJECT_NAME} v${PROJECT_VERSION}"; }

parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -t|--type)       BUILD_TYPE="$2"; shift 2 ;;
            -a|--arch)       ARCH="$2"; shift 2 ;;
            -j|--jobs)       JOBS="$2"; shift 2 ;;
            --cc)            CC="$2"; shift 2 ;;
            --cxx)           CXX="$2"; shift 2 ;;
            --toolchain)     TOOLCHAIN_FILE="$2"; shift 2 ;;
            --sysroot)       SYSROOT_DIR="$2"; shift 2 ;;
            --prefix)        INSTALL_PREFIX="$2"; shift 2 ;;
            --lto)           OPT_LTO=1; shift ;;
            --asan)          OPT_ASAN=1; shift ;;
            --tsan)          OPT_TSAN=1; shift ;;
            --ubsan)         OPT_UBSAN=1; shift ;;
            --werror)        OPT_WERROR=1; shift ;;
            --static)        OPT_STATIC=1; shift ;;
            -c|--clean)      CLEAN=1; shift ;;
            -i|--install)    INSTALL=1; shift ;;
            -p|--package)    PACKAGE=1; shift ;;
            -n|--dry-run)    DRY_RUN=1; shift ;;
            -v|--verbose)    VERBOSE=1; shift ;;
            -h|--help)       usage; exit 0 ;;
            -V|--version)    show_version; exit 0 ;;
            -*)              CMAKE_EXTRA+=("$1"); shift ;;
            *)               die "未知参数: $1" ;;
        esac
    done
}

# ============================================================================
# 检查
# ============================================================================
resolve_arch() {
    # native → 使用本机架构名
    if [[ "${ARCH}" == "native" ]]; then
        ARCH="${HOST_ARCH}"
    fi

    # 如果在 ARM64 主机上指定 aarch64，视为本机编译
    if [[ "${ARCH}" == "aarch64" && "${HOST_ARCH}" == "aarch64" ]]; then
        log "本机即为 aarch64，使用本机编译"
    fi

    # 如果在 x86_64 主机上指定 x86_64，视为本机编译
    if [[ "${ARCH}" == "x86_64" && "${HOST_ARCH}" == "x86_64" ]]; then
        log "本机即为 x86_64，使用本机编译"
    fi
}

is_cross_compile() {
    [[ "${ARCH}" != "${HOST_ARCH}" ]]
}

check_requirements() {
    local missing=()
    for cmd in cmake make; do
        command -v "$cmd" &>/dev/null || missing+=("$cmd")
    done

    if is_cross_compile; then
        local prefix
        case "${ARCH}" in
            aarch64)  prefix="aarch64-linux-gnu" ;;
            x86_64)   prefix="x86_64-linux-gnu" ;;
            *)        prefix="${ARCH}-linux-gnu" ;;
        esac
        CC="${CC:-${prefix}-gcc}"
        CXX="${CXX:-${prefix}-g++}"
    else
        CC="${CC:-gcc}"; CXX="${CXX:-g++}"
    fi

    command -v "${CC}"  &>/dev/null || missing+=("${CC}")
    command -v "${CXX}" &>/dev/null || missing+=("${CXX}")

    if [[ ${#missing[@]} -gt 0 ]]; then
        err "缺少工具: ${missing[*]}"
        if is_cross_compile; then
            echo "安装交叉编译器 (Ubuntu):"
            case "${ARCH}" in
                aarch64) echo "  sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu cmake make" ;;
                x86_64)  echo "  sudo apt install gcc-x86-64-linux-gnu g++-x86-64-linux-gnu cmake make" ;;
            esac
        else
            echo "安装编译器 (Ubuntu):"
            echo "  sudo apt install gcc g++ cmake make"
        fi
        exit 1
    fi

    ok "环境检查通过 (目标: ${ARCH}, 模式: $(is_cross_compile && echo 交叉编译 || echo 本机编译))"
}

# ============================================================================
# 构建
# ============================================================================
get_build_dir() {
    local dir_name
    case "${ARCH}" in
        x86_64|amd64) dir_name="x86" ;;
        *)            dir_name="${ARCH}" ;;
    esac
    echo "build-${dir_name}"
}

clean_build() {
    log "清理构建目录..."
    rm -rf build-* CMakeCache.txt CMakeFiles cmake_install.cmake Makefile
    ok "已清理"
}

configure_and_build() {
    local BUILD_DIR
    BUILD_DIR="$(get_build_dir)"
    JOBS="${JOBS:-$(nproc_fallback)}"

    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    export NARNAT_CC="${CC}"
    export NARNAT_CXX="${CXX}"

    # CMake 参数
    local cmake_args=(
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
        -DCMAKE_C_COMPILER="${CC}"
        -DCMAKE_CXX_COMPILER="${CXX}"
    )

    # 交叉编译设置
    if is_cross_compile; then
        cmake_args+=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR="${ARCH}")

        # 自动选择工具链文件
        if [[ -n "${TOOLCHAIN_FILE}" ]]; then
            cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}")
        elif [[ -f "${SCRIPT_DIR}/toolchain-${ARCH}.cmake" ]]; then
            cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/toolchain-${ARCH}.cmake")
        fi

        if [[ -n "${SYSROOT_DIR}" ]]; then
            cmake_args+=(-DNARNAT_SYSROOT="${SYSROOT_DIR}")
        fi
    else
        cmake_args+=(-DCMAKE_SYSTEM_PROCESSOR="${ARCH}")
    fi

    # 选项开关
    (( OPT_LTO ))     && cmake_args+=(-DNARNAT_ENABLE_LTO=ON)
    (( OPT_ASAN ))    && cmake_args+=(-DNARNAT_ENABLE_ASAN=ON)
    (( OPT_TSAN ))    && cmake_args+=(-DNARNAT_ENABLE_TSAN=ON)
    (( OPT_UBSAN ))   && cmake_args+=(-DNARNAT_ENABLE_UBSAN=ON)
    (( OPT_WERROR ))  && cmake_args+=(-DNARNAT_WARNINGS_AS_ERROR=ON)
    (( OPT_STATIC ))  && cmake_args+=(-DNARNAT_STATIC_LINK=ON)

    cmake_args+=("${CMAKE_EXTRA[@]}")
    cmake_args+=("..")

    # 配置
    log "CMake 配置..."
    if [[ "${DRY_RUN}" == 1 ]]; then
        echo "  cmake ${cmake_args[*]}"
    else
        cmake "${cmake_args[@]}" || { cd ..; die "CMake 配置失败"; }
        ok "CMake 配置完成"
    fi

    # 编译
    log "编译 (j=${JOBS})..."
    if [[ "${DRY_RUN}" == 1 ]]; then
        echo "  make -j${JOBS}"
    else
        local verbose_flag=""
        [[ "${VERBOSE}" == 1 ]] && verbose_flag="VERBOSE=1"
        make -j"${JOBS}" ${verbose_flag} || { cd ..; die "编译失败"; }
        ok "编译完成"
    fi

    # 安装
    if (( INSTALL )); then
        log "安装到 ${INSTALL_PREFIX}..."
        make install || { cd ..; die "安装失败"; }
        ok "安装完成"
    fi

    # 打包
    if (( PACKAGE )); then
        log "打包..."
        cpack -G ZIP || warn "打包失败"
    fi

    cd ..
}

# ============================================================================
# 主流程
# ============================================================================
main() {
    cd "${SCRIPT_DIR}"
    parse_args "$@"
    resolve_arch

    echo "=========================================="
    echo "  ${PROJECT_NAME} v${PROJECT_VERSION}"
    echo "  构建: ${BUILD_TYPE} | 架构: ${ARCH}"
    if is_cross_compile; then
    echo "  模式: 交叉编译 (主机: ${HOST_ARCH} → 目标: ${ARCH})"
    else
    echo "  模式: 本机编译 (${ARCH})"
    fi
    local extras=()
    (( OPT_LTO ))     && extras+=("LTO")
    (( OPT_ASAN ))    && extras+=("ASan")
    (( OPT_TSAN ))    && extras+=("TSan")
    (( OPT_UBSAN ))   && extras+=("UBSan")
    (( OPT_WERROR ))  && extras+=("Werror")
    (( OPT_STATIC ))  && extras+=("Static")
    if [[ ${#extras[@]} -gt 0 ]]; then
    echo "  附加: ${extras[*]}"
    fi
    echo "=========================================="

    if (( CLEAN )); then
        clean_build
        exit 0
    fi

    check_requirements
    configure_and_build

    echo ""
    ok "全部完成"
    local BUILD_DIR; BUILD_DIR="$(get_build_dir)"
    log "可执行文件: ${BUILD_DIR}/${PROJECT_NAME}"
    if [[ -f "${BUILD_DIR}/${PROJECT_NAME}" ]]; then
        file "${BUILD_DIR}/${PROJECT_NAME}" 2>/dev/null || true
    fi
}

main "$@"
