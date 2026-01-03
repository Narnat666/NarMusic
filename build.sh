#!/bin/bash

# C++ HTTP Server 交叉编译构建脚本 (ARM aarch64)
# 用法: ./build.sh [clean]

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印彩色信息
info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示帮助信息
show_help() {
    echo "用法: $0 [选项]"
    echo "选项:"
    echo "  clean     清理构建目录"
    echo "  help      显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  $0          # 正常构建"
    echo "  $0 clean    # 清理构建目录"
    echo "  $0 help     # 显示帮助"
}

# 检查必要工具
check_requirements() {
    info "检查构建工具..."
    
    local missing_tools=()
    
    # 检查 CMake
    if ! command -v cmake &> /dev/null; then
        missing_tools+=("cmake")
    fi
    
    # 检查 make
    if ! command -v make &> /dev/null; then
        missing_tools+=("make")
    fi
    
    # 检查交叉编译工具链
    if [[ ! -f "./toolchain-aarch64.cmake" ]]; then
        error "交叉编译工具链文件 './toolchain-aarch64.cmake' 不存在"
        exit 1
    fi
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        error "缺少必要的构建工具: ${missing_tools[*]}"
        echo "请安装缺少的工具后重试"
        exit 1
    fi
    
    success "所有构建工具检查通过"
}

# 清理构建目录
clean_build() {
    info "清理构建目录..."
    
    if [ -d "build-aarch64" ]; then
        rm -rf build-aarch64
        rm -rf http_server
        rm -rf /mnt/hgfs/tanran/share/*
        success "构建目录已清理"
    else
        warning "构建目录不存在，无需清理"
    fi

    if [ -d "build" ]; then
        rm -rf build
        success "构建目录已清理"
    else
        warning "构建目录不存在，无需清理"
    fi
}

# 主构建函数
build_project() {
    info "开始构建 ARM aarch64 版本..."
    
    # 创建构建目录
    if [ ! -d "build-aarch64" ]; then
        mkdir build-aarch64
        success "创建构建目录: build-aarch64"
    fi
    
    # 进入构建目录
    cd build-aarch64
    
    # 运行 CMake 配置
    info "运行 CMake 配置..."
    cmake -D CMAKE_TOOLCHAIN_FILE=../toolchain-aarch64.cmake ..
    
    if [ $? -eq 0 ]; then
        success "CMake 配置成功"
    else
        error "CMake 配置失败"
        exit 1
    fi
    
    # 获取 CPU 核心数用于并行编译
    local cpu_cores=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    info "检测到 ${cpu_cores} 个 CPU 核心，使用并行编译"
    
    # 编译项目
    info "开始编译..."
    make -j${cpu_cores}
    
    if [ $? -eq 0 ]; then
        success "编译成功完成！"
        cp http_server ../
        cp http_server /mnt/hgfs/tanran/share/ -rf
        # 显示生成的可执行文件
        if [ -f "http_server" ]; then
            echo ""
            info "生成的可执行文件:"
            ls -lh http_server
            file http_server
        fi
    else
        error "编译失败"
        exit 1
    fi
    
    # 返回原始目录
    cd ..
}

# 主函数
main() {
    echo "=========================================="
    echo "  C++ HTTP Server 交叉编译构建脚本"
    echo "  目标架构: ARM aarch64"
    echo "=========================================="
    echo ""
    
    # 处理命令行参数
    case "${1:-}" in
        clean)
            clean_build
            ;;
        help|--help|-h)
            show_help
            exit 0
            ;;
        "")
            # 正常构建流程
            check_requirements
            build_project
            
            echo ""
            success "构建完成！"
            ;;
        *)
            error "未知参数: $1"
            echo ""
            show_help
            exit 1
            ;;
    esac
}

# 运行主函数
main "$@"