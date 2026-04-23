cmake_minimum_required(VERSION 3.10)
project(NarMusic)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 可执行文件
add_executable(NarMusic
    src/main.cpp
    src/http_server.cpp
    src/http_request.cpp
    src/music_analysis.cpp
    src/task_manger.cpp
    src/stream_send.cpp
    src/lyrics_encapsulator.cpp
    src/bili_searcher.cpp
)

target_include_directories(NarMusic PRIVATE
    include
    include/curl
    include/narnat
    include/nlohmann
    include/threadpool
    include/taglib
    include/taglib/toolkit
    include/gumbo-parser
)

target_compile_options(NarMusic PRIVATE
    -Wall
    -Wextra
    -O2
)

# 静态编译程序

target_link_libraries(NarMusic
    ${CMAKE_SOURCE_DIR}/lib/libcurl.a
    ${CMAKE_SOURCE_DIR}/lib/libssl.a
    ${CMAKE_SOURCE_DIR}/lib/libcrypto.a
    ${CMAKE_SOURCE_DIR}/lib/libz.a
    ${CMAKE_SOURCE_DIR}/lib/libgsasl.a
    ${CMAKE_SOURCE_DIR}/lib/libtag.a
    ${CMAKE_SOURCE_DIR}/lib/libgumbo.a
    pthread
    ${CMAKE_DL_LIBS} 
)

# 动态编译程序

# target_link_libraries(http_server
#     ${CMAKE_SOURCE_DIR}/lib/libcurl.so
#     ${CMAKE_SOURCE_DIR}/lib/libssl.so
#     ${CMAKE_SOURCE_DIR}/lib/libcrypto.so
#     ${CMAKE_SOURCE_DIR}/lib/libz.so
#     ${CMAKE_SOURCE_DIR}/lib/libgsasl.so.7
#     ${CMAKE_SOURCE_DIR}/lib/libtag.so.2.1.1
#     pthread
# )