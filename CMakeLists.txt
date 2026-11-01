cmake_minimum_required(VERSION 3.21)

# Set this before project() so CMP0091 controls the MSVC runtime selection.
cmake_policy(SET CMP0091 NEW)

project(MicrostructureStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use the static MSVC runtime (/MT) for portable native deployment.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_program(DOTNET_EXECUTABLE NAMES dotnet REQUIRED)

set(MS_DOTNET_CONFIGURATION "Release" CACHE STRING "Managed build configuration")
set(MS_DOTNET_RID "win-x64" CACHE STRING ".NET runtime identifier")
option(MS_DOTNET_SELF_CONTAINED "Publish with the .NET runtime included" ON)

add_subdirectory(native)

set(MANAGED_PROJECT "${CMAKE_SOURCE_DIR}/managed/MicrostructureStudio.csproj")
set(DOTNET_PUBLISH_DIR "${CMAKE_BINARY_DIR}/dotnet_publish")

if(MS_DOTNET_SELF_CONTAINED)
    set(DOTNET_DEPLOYMENT_ARGS --self-contained true)
else()
    set(DOTNET_DEPLOYMENT_ARGS --self-contained false)
endif()

add_custom_target(publish_dotnet
    COMMAND "${DOTNET_EXECUTABLE}" publish "${MANAGED_PROJECT}"
        -c "${MS_DOTNET_CONFIGURATION}"
        -r "${MS_DOTNET_RID}"
        ${DOTNET_DEPLOYMENT_ARGS}
        -p:UseAppHost=true
        -o "${DOTNET_PUBLISH_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:MicrostructureCore>"
        "${DOTNET_PUBLISH_DIR}/MicrostructureCore.dll"
    DEPENDS MicrostructureCore
    COMMENT "Publishing Microstructure Studio and copying the native engine"
    VERBATIM
)

message(STATUS "Managed configuration : ${MS_DOTNET_CONFIGURATION}")
message(STATUS ".NET runtime identifier: ${MS_DOTNET_RID}")
message(STATUS "Self-contained publish : ${MS_DOTNET_SELF_CONTAINED}")
