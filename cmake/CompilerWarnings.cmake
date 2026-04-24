# ============================================================================
# 编译器警告配置
# 为 GCC/Clang 提供统一的警告级别
# ============================================================================

function(target_set_warnings TARGET)
    cmake_parse_arguments(ARG "AS_ERROR" "" "" ${ARGN})

    set(MSVC_WARNINGS
        /W4     # 高警告级别
        /w14242 # 'identifier': conversion from 'type1' to 'type1', possible loss of data
        /w14254 # 'operator': conversion from 'type1:field1' to 'type2:field2'
        /w14263 # 'function': member function has no formal parameter 'parameter'
        /w14265 # 'class': type needs to have dll-interface to be accessible to clients
        /w14311 # 'variable': pointer truncation from 'type1' to 'type2'
        /w14546 # comma operator before sequence point
        /w14547 # 'operator': operator before comma has no effect; expected operator with side-effect
        /w14549 # 'operator': operator before comma has no effect
        /w14555 # expression has no effect; expected expression with side-effect
        /w14619 # pragma warning: there is no warning number 'number'
        /w14640 # Enable warning on thread un-safe static member initialization
        /w14826 # Conversion from 'type1' to 'type_2' is sign-extended
        /w14905 # LNK4221: no public symbols found; archive member will be inaccessible
        /w14906 # LNK4221: no public symbols found; archive member will be inaccessible
        /w14928 # illegal copy-initialization; more than one user-defined conversion
    )

    set(CLANG_WARNINGS
        -Wall
        -Wextra
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )

    set(GCC_WARNINGS
        ${CLANG_WARNINGS}
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )

    if(ARG_AS_ERROR)
        set(WARNING_FLAG "-Werror")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(WARNINGS ${MSVC_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(WARNINGS ${CLANG_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(WARNINGS ${GCC_WARNINGS})
    else()
        message(AUTHOR_WARNING "未知编译器，仅使用 -Wall -Wextra -Wpedantic")
        set(WARNINGS -Wall -Wextra -Wpedantic)
    endif()

    target_compile_options(${TARGET} PRIVATE ${WARNING_FLAG} ${WARNINGS})
endfunction()
