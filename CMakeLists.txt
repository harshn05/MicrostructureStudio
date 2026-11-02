cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0091 NEW)

project(MicrostructureStudio LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC Runtime
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

# Default Build Configuration
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# .NET Configuration
find_program(DOTNET_EXECUTABLE NAMES dotnet REQUIRED)

set(MS_DOTNET_CONFIGURATION Release CACHE STRING "Managed configuration")
set(MS_DOTNET_RID win-x64 CACHE STRING "Runtime identifier")

option(MS_DOTNET_SELF_CONTAINED "Self-contained publish" ON)

# Native Engine
add_subdirectory(native)

# Managed Application
set(
    MANAGED_PROJECT
    "${CMAKE_SOURCE_DIR}/managed/MicrostructureStudio.csproj"
)

set(
    PUBLISH_DIR
    "${CMAKE_BINARY_DIR}/dotnet_publish"
)

# Deployment Arguments
if(MS_DOTNET_SELF_CONTAINED)
    set(DEPLOY_ARGS --self-contained true)
else()
    set(DEPLOY_ARGS --self-contained false)
endif()

# .NET Publish Target
add_custom_target(publish_dotnet
    COMMAND
        "${DOTNET_EXECUTABLE}" publish
        "${MANAGED_PROJECT}"
        -c "${MS_DOTNET_CONFIGURATION}"
        -r "${MS_DOTNET_RID}"
        ${DEPLOY_ARGS}
        -p:UseAppHost=true
        -o "${PUBLISH_DIR}"

    COMMAND
        "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:MicrostructureCore>"
        "${PUBLISH_DIR}/MicrostructureCore.dll"

    DEPENDS
        MicrostructureCore

    VERBATIM
)