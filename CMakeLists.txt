# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.25)

project(isoforge VERSION 1.0.0 LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "isoforge must be built with MSVC.")
endif()

# Check the target architecture reported by the compiler, not just the pointer size. The presets also
# name the architecture they build for (ISOFORGE_ARCH), and it must match: a preset run in the
# developer environment of the other architecture would otherwise build the wrong executable.
# Without a preset, the compiler's architecture is used.
string(TOLOWER "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" isoforge_compiler_arch)
if(NOT isoforge_compiler_arch MATCHES "^(x64|x86)$")
  message(FATAL_ERROR
    "isoforge must be built for x64 or x86 (compiler architecture: '${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}'). "
    "Use a VsDevCmd environment started with -arch=amd64 or -arch=x86.")
endif()
if(NOT ISOFORGE_ARCH)
  set(ISOFORGE_ARCH "${isoforge_compiler_arch}")
elseif(NOT ISOFORGE_ARCH STREQUAL isoforge_compiler_arch)
  message(FATAL_ERROR
    "The preset builds for '${ISOFORGE_ARCH}', but the compiler targets '${isoforge_compiler_arch}'. "
    "Start VsDevCmd with -arch=amd64 for x64 presets and -arch=x86 for x86 presets (scripts\\build.cmd does this).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# /MTd for Debug, /MT for Release.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Compile settings shared by every target in this project.
add_library(isoforge_options INTERFACE)
target_compile_options(isoforge_options INTERFACE /W4 /permissive- /utf-8)
target_compile_definitions(isoforge_options INTERFACE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

configure_file(src/isoforge_version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/isoforge_version.h" @ONLY)

add_library(isoforge_core STATIC
  src/cli/app.cpp
  src/cli/command_line.cpp
  src/cli/console.cpp
  src/cli/create_command.cpp
  src/cli/device_errors.cpp
  src/cli/drive_provider.cpp
  src/cli/help.cpp
  src/cli/info_command.cpp
  src/cli/json_values.cpp
  src/cli/list_command.cpp
  src/cli/reporting.cpp
  src/cli/services.cpp
  src/cli/style.cpp
  src/cli/verify_command.cpp
  src/device/device_error.cpp
  src/device/drive_enumeration.cpp
  src/device/drive_path.cpp
  src/device/optical_drive.cpp
  src/device/scsi_commands.cpp
  src/device/scsi_device.cpp
  src/device/scsi_sense.cpp
  src/device/scsi_transport.cpp
  src/fs/filesystem.cpp
  src/imaging/range_decision.cpp
  src/imaging/reader.cpp
  src/media/assessment.cpp
  src/media/profile.cpp
  src/media/readiness.cpp
  src/media/responses.cpp
  src/util/file_system.cpp
  src/util/json.cpp
  src/util/keep_awake.cpp
  src/util/sha256.cpp
  src/util/strings.cpp
  src/util/utf8.cpp
  src/util/version.cpp
)
target_include_directories(isoforge_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(isoforge_core
  PUBLIC isoforge_options
  PUBLIC bcrypt
)

add_executable(isoforge
  src/main.cpp
  src/app.manifest
)
target_link_libraries(isoforge PRIVATE isoforge_core)
# app.manifest carries the requestedExecutionLevel; do not let the linker add another one.
# /DEPENDENTLOADFLAG:0x800 (LOAD_LIBRARY_SEARCH_SYSTEM32) makes the loader take the imported DLLs
# that are not KnownDLLs (bcrypt.dll) only from System32, never from the executable's folder, so a
# DLL planted next to an extracted isoforge.exe is not loaded.
target_link_options(isoforge PRIVATE /MANIFESTUAC:NO /DEPENDENTLOADFLAG:0x800)

# Distribution zip: isoforge-<version>-win-<x64|x86>.zip with the executable, LICENSE and README at
# its root. Create it with "cpack --preset x64-release" or "cpack --preset x86-release".
install(TARGETS isoforge RUNTIME DESTINATION .)
install(FILES LICENSE README.md DESTINATION .)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME isoforge)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "isoforge-${PROJECT_VERSION}-win-${ISOFORGE_ARCH}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
