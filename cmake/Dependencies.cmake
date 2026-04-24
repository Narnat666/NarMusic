# ============================================================================
# 依赖库配置
# 统一管理所有第三方库的路径、头文件和链接
# ============================================================================

# 静态库根目录
set(NARNAT_LIB_DIR "${CMAKE_SOURCE_DIR}/lib" CACHE PATH "第三方静态库目录")
set(NARNAT_INC_DIR "${CMAKE_SOURCE_DIR}/include" CACHE PATH "第三方头文件目录")

# ---------------------------------------------------------------------------
# 验证所有必需的静态库存在
# ---------------------------------------------------------------------------
function(validate_static_libraries)
    set(REQUIRED_LIBS
        libcurl.a libssl.a libcrypto.a libz.a
        libgsasl.a libtag.a libgumbo.a libsqlite3.a
    )
    foreach(LIB ${REQUIRED_LIBS})
        if(NOT EXISTS "${NARNAT_LIB_DIR}/${LIB}")
            message(FATAL_ERROR "缺少静态库: ${NARNAT_LIB_DIR}/${LIB}")
        endif()
    endforeach()
    message(STATUS "所有静态库验证通过")
endfunction()

# ---------------------------------------------------------------------------
# 配置目标依赖
# ---------------------------------------------------------------------------
function(target_link_dependencies TARGET)
    # 头文件搜索路径
    target_include_directories(${TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${NARNAT_INC_DIR}
        ${NARNAT_INC_DIR}/curl
        ${NARNAT_INC_DIR}/sqlite
        ${NARNAT_INC_DIR}/nlohmann
        ${NARNAT_INC_DIR}/threadpool
        ${NARNAT_INC_DIR}/taglib
        ${NARNAT_INC_DIR}/taglib/toolkit
        ${NARNAT_INC_DIR}/gumbo-parser
        ${NARNAT_INC_DIR}/zlib
    )

    # 静态链接
    target_link_libraries(${TARGET} PRIVATE
        ${NARNAT_LIB_DIR}/libcurl.a
        ${NARNAT_LIB_DIR}/libssl.a
        ${NARNAT_LIB_DIR}/libcrypto.a
        ${NARNAT_LIB_DIR}/libz.a
        ${NARNAT_LIB_DIR}/libgsasl.a
        ${NARNAT_LIB_DIR}/libtag.a
        ${NARNAT_LIB_DIR}/libgumbo.a
        ${NARNAT_LIB_DIR}/libsqlite3.a
        pthread
        ${CMAKE_DL_LIBS}
    )
endfunction()
