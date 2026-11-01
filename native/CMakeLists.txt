add_library(MicrostructureCore SHARED
    MicrostructureCore.cpp
    MicrostructureCore.h
)

target_compile_definitions(MicrostructureCore
    PRIVATE MICROSTRUCTURECORE_EXPORTS
)

set_target_properties(MicrostructureCore PROPERTIES
    PREFIX ""
    OUTPUT_NAME "MicrostructureCore"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native_bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native_bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native_lib"
)

if(MSVC)
    target_compile_options(MicrostructureCore PRIVATE
        /W4
        /permissive-
    )
elseif(MINGW)
    target_compile_options(MicrostructureCore PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    # Statically link the MinGW compiler runtimes where supported by the toolchain.
    target_link_options(MicrostructureCore PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
else()
    target_compile_options(MicrostructureCore PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
