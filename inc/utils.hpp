/*
 * MIT License
 *
 * Copyright (c) 2025 Dominik Protasewicz
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

#pragma once

#include <windows.h>
#include <vector>
#include <string>

namespace Utils
{
    /**
     * @brief Retrieves information about the compiler being used.
     * @details This function returns a string containing the name and version of the
     * compiler. It checks for several well-known compilers and formats the version
     * information accordingly:
     *
     * - **GCC:** The version is formatted as "GCC - major.minor.patch".
     * - **Clang:** The version is formatted as "Clang - major.minor.patch".
     * - **MSVC:** The version is formatted as "MSVC - version number".
     * - **Unknown Compiler:** If the compiler is not recognized, it returns "UNKNOWN".
     *
     * @return `std::string` containing the compiler name and version.
     */
    std::string getCompilerInfo();

    /**
     * @brief Convert memory bytes into string representation
     * @details Converts the bytes pointed to by the `bytes` parameter into a string.
     *      The total number of bytes that will be converted is based on the `size`
     *      parameter. The returned string will be in hexidecimal format and will be
     *      organized as the bytes appear in memory. For example, if the `bytes` parameter
     *      points to some integer in memory equal to `0x12345678`, and the `size` parameter
     *      is given sizeof(int), then the returned string shall be "78 56 34 12", as that is
     *      how `0x12345678` is stored in memory, on little endian x86_64.
     *
     * @param bytes Pointer to memory
     * @param size Size of `bytes` parameter
     * @return std::string
     *
     * @code
     * float a = 3.5555556; // In hex: 0x40638E39
     * std::string string = bytesToString(&a, sizeof(float));
     * std::cout << string << std::endl; // Prints "39 8E 63 40"
     * @endcode
     */
    std::string bytesToString(void* bytes, size_t size);

    /**
     * @brief Get the width and height, respectively, of the desktop in pixels
     *
     * @return std::pair<int, int>
     */
    std::pair<int, int> GetDesktopDimensions();

    /**
     * @brief Patch an area of memory with a pattern
     * @details Patches an area of memory pointed to by `address` with the pattern.
     *      The `pattern` parameter is expected to be in the form of a hex string as such:
     *      "DE AD BE EF". There is no limit is how long the pattern string can be, but
     *      it is important to be mindful as to not go out of bounds of the program space
     *      else a segmentation fault will occur. The total amount of memory that will
     *      be patched is determined by the size of the `pattern` parameter. If the example
     *      hex string above is referenced then the total amount of memory that will be
     *      patched is 4 bytes, starting at the address[0] and ending at the address[3].
     *
     * @param address Starting memory address
     * @param pattern IDA-style byte array pattern
     */
    void patch(uintptr_t address, const char* pattern);

    /**
     * @brief Scan for a given byte pattern on a module
     * @details Obtained and modified from:
     *      https://github.com/OneshotGH/CSGOSimple-master/blob/master/CSGOSimple/helpers/utils.cpp
     *      Original implementation is for the most part intact. Modified so that all
     *      the addresses where the pattern is found is appended to the `address` vector,
     *      instead of returning the address when the first instance is found.
     *
     * @param module Base of the module to search
     * @param signature IDA-style byte array pattern
     * @param address Vector of addresses where the pattern was found
     */
    void patternScan(void* module, const char* signature, std::vector<uint64_t>* address);
}
