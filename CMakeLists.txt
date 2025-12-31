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
    src/task_manger.cpp
)

target_include_directories(http_server PRIVATE
    include
    include/curl
    include/narnat
    include/nlohmann
)

target_compile_options(http_server PRIVATE
    -Wall
    -Wextra
    -O2
)

target_link_libraries(http_server
    ${CMAKE_SOURCE_DIR}/lib/libcurl.so
    ${CMAKE_SOURCE_DIR}/lib/libssl.so
    ${CMAKE_SOURCE_DIR}/lib/libcrypto.so
    ${CMAKE_SOURCE_DIR}/lib/libz.so
    ${CMAKE_SOURCE_DIR}/lib/libgsasl.so.7
    
    pthread
)