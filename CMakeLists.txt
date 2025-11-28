cmake_minimum_required(VERSION 3.10)
project(http_server)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件，包含所有必要的源文件
add_executable(http_server 
    src/main.cpp 
    src/http_server.cpp
    src/http_request.cpp
)

# 对于旧版本编译器，可能需要链接文件系统库
# target_link_libraries(http_server stdc++fs)