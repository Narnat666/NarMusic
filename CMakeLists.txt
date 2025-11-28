cmake_minimum_required(VERSION 3.10)

# 交叉编译工具链设置（如果提供了工具链文件）
if(CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

project(http_server)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(http_server 
    src/main.cpp 
    src/http_server.cpp
    src/http_request.cpp
)

# 根据编译器类型决定是否链接文件系统库
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # 检查GCC版本，>= 9 通常不需要显式链接
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
        target_link_libraries(http_server stdc++fs)
    endif()
endif()