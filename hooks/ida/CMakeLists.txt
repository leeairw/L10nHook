set(IDA_HOOK_SOURCES
    dllmain.cpp
    Hook.h
    Hook.cpp
)

foreach(proxy_name IN LISTS HOOK_PROXY_NAMES)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(proxy_source ${CMAKE_SOURCE_DIR}/proxy/${proxy_name}_x64.cpp)
    else()
        set(proxy_source ${CMAKE_SOURCE_DIR}/proxy/${proxy_name}_x86.cpp)
    endif()

    if(NOT EXISTS ${proxy_source})
        message(FATAL_ERROR "Missing proxy source for ${proxy_name}: ${proxy_source}")
    endif()

    set(target_name ida_${proxy_name})
    add_library(${target_name} SHARED
        ${IDA_HOOK_SOURCES}
        ${proxy_source}
    )

    set_target_properties(${target_name}
        PROPERTIES
            OUTPUT_NAME ${proxy_name}
            FOLDER "hooks/ida"
    )
    hook_set_dll_output(${target_name} ida)

    target_link_libraries(${target_name}
        PRIVATE
            common
            detours
            user32
    )

    target_compile_definitions(${target_name}
        PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
    )

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    endif()
endforeach()
