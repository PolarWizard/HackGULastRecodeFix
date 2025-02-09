# MIT License
#
# Copyright (c) 2025 Dominik Protasewicz
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.28)

# Variables
set(PROJECT_NAME HackGULastRecodeFix)
set(DEFAULT_GAME_FOLDER "C:/Program Files (x86)/Steam/steamapps/common/hackGU")
set(GAME_FOLDER "${DEFAULT_GAME_FOLDER}" CACHE STRING "User specified path to game folder")
if (EXISTS "${GAME_FOLDER}/hackGU.exe")
    message(STATUS "Game folder: ${GAME_FOLDER}")
else()
    message(FATAL_ERROR "Bad game folder provided: ${GAME_FOLDER}")
endif()

# Force all MSVC Runtimes to be linked statically into DLL
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)

# Set the project name and version
project(${PROJECT_NAME} VERSION 1.0)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Define the common output directory
set(OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Configure debug and release directories separately
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIRECTORY}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIRECTORY}/Release)

# Add DLL
set(DLL_FILES src/dllmain.cpp src/utils.cpp)
add_library(${PROJECT_NAME} SHARED ${DLL_FILES})

# Add /utf-8 flag for MSVC
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
endif()

# Add directory and build
add_subdirectory(yaml-cpp EXCLUDE_FROM_ALL)
add_subdirectory(zydis EXCLUDE_FROM_ALL)
add_subdirectory(safetyhook EXCLUDE_FROM_ALL)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    inc
    spdlog/include
    yaml-cpp/include
    safetyhook/include
)

# Include libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Zydis
    yaml-cpp
    safetyhook
)

install(CODE "
    execute_process(
        COMMAND
            powershell.exe
                -ExecutionPolicy Bypass
                -File \"${CMAKE_SOURCE_DIR}/install.ps1\" \"${GAME_FOLDER}\"
    )
")
