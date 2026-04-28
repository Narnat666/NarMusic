# ============================================================================
# NarMusic - C++ Music Server
# 企业级 CMake 构建系统
# ============================================================================

cmake_minimum_required(VERSION 3.16)

# 防止源码内构建
include(cmake/PreventInSourceBuild.cmake)

# ---------------------------------------------------------------------------
# 项目定义
# ---------------------------------------------------------------------------
project(NarMusic
    VERSION     2.0.0
    LANGUAGES   C CXX
    DESCRIPTION "纳儿音乐服务器"
)

# ---------------------------------------------------------------------------
# CMake 策略与标准
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # 禁用编译器扩展，保证可移植性

# 导出 compile_commands.json 供 IDE/工具使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# 加载 CMake 模块
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Version)
include(Dependencies)
include(CompilerWarnings)

# ---------------------------------------------------------------------------
# 构建选项
# ---------------------------------------------------------------------------
option(NARNAT_ENABLE_LTO       "启用链接时优化 (LTO)"       OFF)
option(NARNAT_ENABLE_ASAN      "启用地址消毒器 (ASan)"      OFF)
option(NARNAT_ENABLE_TSAN      "启用线程消毒器 (TSan)"      OFF)
option(NARNAT_ENABLE_UBSAN     "启用未定义行为消毒器 (UBSan)" OFF)
option(NARNAT_WARNINGS_AS_ERROR "将警告视为错误"             OFF)
option(NARNAT_STATIC_LINK      "完全静态链接 (含 libc)"     OFF)

# ---------------------------------------------------------------------------
# 源文件
# ---------------------------------------------------------------------------
set(NARNAT_SOURCES
    # 入口
    src/main.cpp

    # Core 层
    src/core/logger.cpp
    src/core/http/request.cpp
    src/core/http/response.cpp
    src/core/http/router.cpp
    src/core/epoll_server.cpp

    # Config 层
    src/config/config.cpp

    # Domain 层
    src/domain/task.cpp

    # Infrastructure 层 - 持久化
    src/infrastructure/persistence/database.cpp
    src/infrastructure/persistence/sqlite_task_repository.cpp
    src/infrastructure/persistence/sqlite_music_library_repository.cpp

    # Infrastructure 层 - HTTP 客户端
    src/infrastructure/http_client/curl_client.cpp

    # Infrastructure 层 - Bilibili
    src/infrastructure/bilibili/bilibili_client.cpp

    # Infrastructure 层 - 歌词
    src/infrastructure/lyrics/lyrics_aggregator.cpp
    src/infrastructure/lyrics/kugou_provider.cpp
    src/infrastructure/lyrics/netease_provider.cpp
    src/infrastructure/lyrics/qqmusic_provider.cpp
    src/infrastructure/lyrics/qishui_provider.cpp

    # Infrastructure 层 - 音频
    src/infrastructure/audio/audio_downloader.cpp

    # Infrastructure 层 - 流式传输
    src/infrastructure/streaming/stream_sender.cpp

    # Infrastructure 层 - 文件系统
    src/infrastructure/filesystem/music_file_repository.cpp
    src/infrastructure/filesystem/zip_writer.cpp

    # Application 层
    src/application/download_service.cpp
    src/application/search_service.cpp
    src/application/library_service.cpp
    src/application/streaming_service.cpp

    # Presentation 层
    src/presentation/controllers/download_controller.cpp
    src/presentation/controllers/search_controller.cpp
    src/presentation/controllers/library_controller.cpp
    src/presentation/middleware/static_file_handler.cpp
)

# ---------------------------------------------------------------------------
# 构建目标
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${NARNAT_SOURCES})

# 平台定义
target_compile_definitions(${PROJECT_NAME} PRIVATE
    _GNU_SOURCE              # accept4, SO_REUSEPORT 等 GNU 扩展
    $<$<CONFIG:Debug>:DEBUG> # Debug 模式标识
)

# 优化级别
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    target_compile_options(${PROJECT_NAME} PRIVATE -O2)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g3)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(${PROJECT_NAME} PRIVATE -O2 -g1)
endif()

# 警告
if(NARNAT_WARNINGS_AS_ERROR)
    target_set_warnings(${PROJECT_NAME} AS_ERROR)
else()
    target_set_warnings(${PROJECT_NAME})
endif()

# 抑制第三方头文件引入的告警
set_source_files_properties(
    src/infrastructure/bilibili/bilibili_client.cpp
    src/infrastructure/lyrics/kugou_provider.cpp
    src/infrastructure/lyrics/netease_provider.cpp
    src/infrastructure/lyrics/qqmusic_provider.cpp
    src/infrastructure/lyrics/qishui_provider.cpp
    src/presentation/controllers/download_controller.cpp
    src/presentation/controllers/search_controller.cpp
    src/core/http/response.cpp
    PROPERTIES COMPILE_FLAGS "-Wno-null-dereference -Wno-sign-conversion"
)
set_source_files_properties(
    src/infrastructure/filesystem/music_file_repository.cpp
    PROPERTIES COMPILE_FLAGS "-Wno-shadow -Wno-unused-result"
)
set_source_files_properties(
    src/main.cpp
    PROPERTIES COMPILE_FLAGS "-Wno-shadow"
)
set_source_files_properties(
    src/infrastructure/filesystem/zip_writer.cpp
    PROPERTIES COMPILE_FLAGS "-Wno-old-style-cast -Wno-useless-cast"
)

# LTO
if(NARNAT_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_OK OUTPUT IPO_ERR)
    if(IPO_OK)
        set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "LTO 已启用")
    else()
        message(WARNING "LTO 不可用: ${IPO_ERR}")
    endif()
endif()

# 消毒器
set(SAN_FLAGS "")
if(NARNAT_ENABLE_ASAN)
    list(APPEND SAN_FLAGS -fsanitize=address -fno-omit-frame-pointer)
endif()
if(NARNAT_ENABLE_TSAN)
    list(APPEND SAN_FLAGS -fsanitize=thread)
endif()
if(NARNAT_ENABLE_UBSAN)
    list(APPEND SAN_FLAGS -fsanitize=undefined)
endif()
if(SAN_FLAGS)
    target_compile_options(${PROJECT_NAME} PRIVATE ${SAN_FLAGS})
    target_link_options(${PROJECT_NAME} PRIVATE ${SAN_FLAGS})
endif()

# 完全静态链接
if(NARNAT_STATIC_LINK)
    target_link_options(${PROJECT_NAME} PRIVATE -static)
endif()

# ---------------------------------------------------------------------------
# 依赖库
# ---------------------------------------------------------------------------
validate_static_libraries()
target_link_dependencies(${PROJECT_NAME})

# ---------------------------------------------------------------------------
# 版本信息
# ---------------------------------------------------------------------------
generate_version_header(${PROJECT_NAME})

# ---------------------------------------------------------------------------
# 安装规则
# ---------------------------------------------------------------------------
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
install(FILES config.json
    DESTINATION etc/${PROJECT_NAME}
)
install(DIRECTORY web/
    DESTINATION share/${PROJECT_NAME}/web
)

# ---------------------------------------------------------------------------
# CPack 打包
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "NarMusicServer")
set(CPACK_PACKAGE_VERSION ${NARNAT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "纳儿音乐服务器")
set(CPACK_PACKAGE_VENDOR "NarNat")
set(CPACK_PACKAGE_FILE_NAME "NarMusicServer-dist")
set(CPACK_GENERATOR ZIP)
set(CPACK_STRIP_FILES TRUE)
set(CPACK_INSTALL_PREFIX "/opt/${PROJECT_NAME}")

include(CPack)
