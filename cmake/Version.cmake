# ============================================================================
# 版本信息配置
# 从 CMake 变量生成 version.h
# ============================================================================

set(NARNAT_VERSION_MAJOR 1 CACHE STRING "主版本号")
set(NARNAT_VERSION_MINOR 0 CACHE STRING "次版本号")
set(NARNAT_VERSION_PATCH 0 CACHE STRING "补丁版本号")

set(NARNAT_VERSION
    "${NARNAT_VERSION_MAJOR}.${NARNAT_VERSION_MINOR}.${NARNAT_VERSION_PATCH}"
)

# 生成 version.h
function(generate_version_header TARGET)
    set(VERSION_H_IN [=[
#ifndef NARNAT_VERSION_H
#define NARNAT_VERSION_H

#define NARNAT_VERSION_MAJOR  @NARNAT_VERSION_MAJOR@
#define NARNAT_VERSION_MINOR  @NARNAT_VERSION_MINOR@
#define NARNAT_VERSION_PATCH  @NARNAT_VERSION_PATCH@
#define NARNAT_VERSION        "@NARNAT_VERSION@"
#define NARNAT_PROJECT_NAME   "@PROJECT_NAME@"

#endif
]=])

    set(VERSION_H_PATH "${CMAKE_BINARY_DIR}/generated/version.h")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

    file(WRITE "${VERSION_H_PATH}"
        "#ifndef NARNAT_VERSION_H\n"
        "#define NARNAT_VERSION_H\n"
        "\n"
        "#define NARNAT_VERSION_MAJOR  ${NARNAT_VERSION_MAJOR}\n"
        "#define NARNAT_VERSION_MINOR  ${NARNAT_VERSION_MINOR}\n"
        "#define NARNAT_VERSION_PATCH  ${NARNAT_VERSION_PATCH}\n"
        "#define NARNAT_VERSION        \"${NARNAT_VERSION}\"\n"
        "#define NARNAT_PROJECT_NAME   \"${PROJECT_NAME}\"\n"
        "#define NARNAT_TARGET_ARCH    \"${NARNAT_TARGET_ARCH}\"\n"
        "\n"
        "#endif\n"
    )

    target_include_directories(${TARGET} PRIVATE "${CMAKE_BINARY_DIR}/generated")
    message(STATUS "版本: ${NARNAT_VERSION}")
endfunction()
