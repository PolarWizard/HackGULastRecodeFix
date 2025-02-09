/*
 * MIT License
 *
 * Copyright (c) 2024 Dominik Protasewicz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <windows.h>
#include <vector>
#include <format>
#include <iostream>
#include <cstdint>

#include "utils.hpp"

namespace Utils
{
    std::string getCompilerInfo() {
#if defined(__GNUC__)
        std::string compiler = "GCC - "
            + std::to_string(__GNUC__) + "."
            + std::to_string(__GNUC_MINOR__) + "."
            + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(__clang__)
        std::string compiler = "Clang - "
            + std::to_string(__clang_major__) + "."
            + std::to_string(__clang_minor__) + "."
            + std::to_string(__clang_patchlevel__);
#elif defined(_MSC_VER)
        std::string compiler = "MSVC - "
            + std::to_string(_MSC_VER);
#else
        "UNKNOWN"
#endif
        return compiler;
    }

    std::string bytesToString(void* bytes, size_t size) {
        std::string pattern;
        for (size_t i = 0; i < size; i++) {
            pattern += std::format("{:02X} ", ((uint8_t*)bytes)[i]);
        }
        if (!pattern.empty()) {
            pattern.pop_back();
        }
        return pattern;
    }

    std::pair<int, int> GetDesktopDimensions() {
        DEVMODE devMode{};
        devMode.dmSize = sizeof(DEVMODE);
        if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode)) {
            return { devMode.dmPelsWidth, devMode.dmPelsHeight };
        }
        return {};
    }

    void patch(uintptr_t address, const char* pattern)
    {
        static auto pattern_to_byte = [](const char* pattern) {
            auto bytes = std::vector<uint8_t>{};
            auto start = const_cast<char*>(pattern);
            auto end = const_cast<char*>(pattern) + strlen(pattern);
            for (auto current = start; current < end; ++current) {
                bytes.push_back((uint8_t)strtoul(current, &current, 16));
            }
            return bytes;
        };

        DWORD oldProtect;
        auto patternBytes = pattern_to_byte(pattern);
        VirtualProtect((LPVOID)address, patternBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((LPVOID)address, patternBytes.data(), patternBytes.size());
        VirtualProtect((LPVOID)address, patternBytes.size(), oldProtect, &oldProtect);
    }

    void patternScan(void* module, const char* signature, std::vector<uint64_t>* address)
    {
        static auto pattern_to_byte = [](const char* pattern) {
            auto bytes = std::vector<int>{};
            auto start = const_cast<char*>(pattern);
            auto end = const_cast<char*>(pattern) + strlen(pattern);

            for (auto current = start; current < end; ++current) {
                if (*current == '?') {
                    ++current;
                    if (*current == '?')
                        ++current;
                    bytes.push_back(-1);
                }
                else {
                    bytes.push_back(strtoul(current, &current, 16));
                }
            }
            return bytes;
        };

        auto dosHeader = (PIMAGE_DOS_HEADER)module;
        auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);

        auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
        auto patternBytes = pattern_to_byte(signature);
        auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

        auto s = patternBytes.size();
        auto d = patternBytes.data();

        for (auto i = 0ul; i < sizeOfImage - s; ++i) {
            bool found = true;
            for (auto j = 0ul; j < s; ++j) {
                if (scanBytes[i + j] != d[j] && d[j] != -1) {
                    found = false;
                    break;
                }
            }
            if (found) {
                address->push_back((uint64_t)&scanBytes[i]);
            }
        }
    }
}
