cmake_minimum_required(VERSION 3.10)
project(http_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 可执行文件
add_executable(http_server
    src/main.cpp
    src/http_server.cpp
    src/http_request.cpp
    src/music_analysis.cpp
    src/task_manger
)

# 编译器选项
target_compile_options(http_server PRIVATE
    -Wall
    -Wextra
    -Werror
    -O2
)

# 链接库
target_link_libraries(http_server
    pthread
)