# ============================================================================
# NarMusic ARM64 (aarch64) Linux 交叉编译工具链
# ============================================================================

# 系统信息
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_CROSSCOMPILING TRUE)

# ---------------------------------------------------------------------------
# 编译器
# ---------------------------------------------------------------------------
# 优先使用环境变量，其次使用默认交叉编译器
if(NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
endif()
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
endif()

# ---------------------------------------------------------------------------
# 编译标志
# ---------------------------------------------------------------------------
set(ARM_CFLAGS "-march=armv8-a" CACHE STRING "ARM64 编译标志")

string(CONCAT CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${ARM_CFLAGS}")
string(CONCAT CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARM_CFLAGS}")

# ---------------------------------------------------------------------------
# Sysroot（可选）
# ---------------------------------------------------------------------------
if(DEFINED NARNAT_SYSROOT AND EXISTS "${NARNAT_SYSROOT}")
    set(CMAKE_SYSROOT "${NARNAT_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${NARNAT_SYSROOT}")
    message(STATUS "Sysroot: ${NARNAT_SYSROOT}")
endif()

# 查找规则：仅在目标平台搜索库和头文件
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE  ONLY)

# ---------------------------------------------------------------------------
# 编译器可用性验证
# ---------------------------------------------------------------------------
foreach(COMP C CXX)
    execute_process(
        COMMAND ${CMAKE_${COMP}_COMPILER} --version
        RESULT_VARIABLE _RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    if(_RESULT)
        message(FATAL_ERROR
            "${COMP} 编译器不可用: ${CMAKE_${COMP}_COMPILER}\n"
            "安装: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
        )
    endif()
endforeach()
