# ============================================================================
# ARM64 (aarch64) Linux 交叉编译工具链文件
# ============================================================================

# 设置系统信息
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 指定交叉编译器
# 默认使用系统PATH中的交叉编译器
# 可以通过环境变量覆盖：-DCMAKE_C_COMPILER=... -DCMAKE_CXX_COMPILER=...
if(NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
endif()

if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
endif()

# 设置编译器标志
set(CMAKE_C_FLAGS "-march=armv8-a" CACHE STRING "C compiler flags")
set(CMAKE_CXX_FLAGS "-march=armv8-a" CACHE STRING "C++ compiler flags")

# 可选：设置sysroot路径
# 如果交叉编译工具链包含sysroot，可以在这里指定
# set(CMAKE_SYSROOT /path/to/sysroot)

# 查找规则
# 在交叉编译环境中，我们只在sysroot中查找库和头文件
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 设置目标平台属性
set(CMAKE_CROSSCOMPILING TRUE)

# 设置目标系统版本（可选）
# set(CMAKE_SYSTEM_VERSION 1)

# 设置目标特性（可选）
# set(CMAKE_C_COMPILE_FEATURES "c_std_11")
# set(CMAKE_CXX_COMPILE_FEATURES "cxx_std_17")

# ============================================================================
# 项目特定设置
# ============================================================================

# 设置静态链接（如果需要）
# set(CMAKE_EXE_LINKER_FLAGS "-static")

# 设置库搜索路径（如果需要）
# 如果项目依赖特定的ARM64库，可以在这里添加搜索路径
# set(CMAKE_LIBRARY_PATH /path/to/arm64/libs)
# set(CMAKE_INCLUDE_PATH /path/to/arm64/includes)

# ============================================================================
# 编译器检查
# ============================================================================

# 禁用编译器测试（交叉编译时可能需要）
# set(CMAKE_C_COMPILER_WORKS TRUE)
# set(CMAKE_CXX_COMPILER_WORKS TRUE)

# ============================================================================
# 工具链验证
# ============================================================================

# 验证编译器是否存在
execute_process(
    COMMAND ${CMAKE_C_COMPILER} --version
    RESULT_VARIABLE GCC_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)

if(GCC_RESULT)
    message(WARNING "C编译器 ${CMAKE_C_COMPILER} 未找到或不可用")
endif()

execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} --version
    RESULT_VARIABLE GXX_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)

if(GXX_RESULT)
    message(WARNING "C++编译器 ${CMAKE_CXX_COMPILER} 未找到或不可用")
endif()

# ============================================================================
# 使用说明
# ============================================================================
# 使用此工具链文件：
# 1. 确保已安装ARM64交叉编译工具链
#    Ubuntu/Debian: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
# 2. 使用CMake配置：
#    cmake -DCMAKE_TOOLCHAIN_FILE=toolchain-aarch64.cmake ..
# 3. 或者使用构建脚本：
#    ./build-arm64.sh
# ============================================================================