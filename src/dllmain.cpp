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

/**
 * @file dllmain.cpp
 * @brief Hack GU Last Recode Fix
 *
 * @note This DLL must spin indifinitely until the game exe itself closes and cleans
 * everything up. Typically with these kind of ASI fixes the DLL gets loaded early
 * into game code, injects and/or patches stuff, and unloads itself. For this game
 * we cannot do that given the structure of the game.
 *
 * The game is made of one exe and multiple DLL's where these DLL's are the actual game
 * code and data files. This DLL must spin indefinitely and keep track of what DLL is
 * currently loaded, inject the hooks and patches into that DLL, and then wait until
 * the DLL is unloaded, and rinse and repeat until the exe is closed down effectively
 * also forcefully terminating this DLL.
 *
 * This DLL will actually cause a memory leak! But dont worry, it is practically
 * impossible for the game to crash because of it. The memory leak happens because we are
 * constantly appending safetyhook objects in the `centerUiHook` vector. These objects
 * we can never truly free because they contain pointers to hook locations that will
 * no longer exist when the game unloads the current loaded DLL. And when a new DLL
 * takes it place well the fix doesn't create a new hook as the object already exists
 * and the safetyhook library thinks the hook is still valid, my theory anyway. So
 * basically the fix will not work, when a new DLL is loaded say the chapter 2 DLL.
 * So we need to append the safetyhook objects into a vector so that we may keep
 * creating new ones. The reason we will never crash is because you would need to keep
 * changing DLL's a gazillion times over and then some to really make a dent in your
 * computers avaiable memory because Windows says "that's enough" and terminates the
 * process, humanly this is practically impossible to do, therefore we can cause this
 * leak to happen and never worry about any consequences from it.
 *
 */

// System includes
#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <format>
#include <numeric>
#include <numbers>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <bit>

// 3rd party includes
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "yaml-cpp/yaml.h"
#include "safetyhook.hpp"

// Local includes
#include "utils.hpp"

// Macros
#define VERSION "1.0.1"
#define LOG(STRING, ...) spdlog::info("{} : " STRING, __func__, ##__VA_ARGS__)

// .yml to struct
typedef struct resolution_t {
    int width;
    int height;
    float aspectRatio;
} resolution_t;

typedef struct combatOverlay_t {
    bool enable;
} combatOverlay_t;

typedef struct feature_t {
    combatOverlay_t combatOverlay;
} feature_t;

typedef struct yml_t {
    std::string name;
    bool masterEnable;
    resolution_t resolution;
    feature_t feature;
} yml_t;

// Globals
HMODULE mainModule = GetModuleHandle(NULL);
HMODULE baseModule = GetModuleHandle(NULL);
std::string strBaseModule;
int normalizedWidth = 0;
int normalizedOffset = 0;
float widthScalingFactor = 0;
std::vector<SafetyMidHook> centerUiHook;
YAML::Node config = YAML::LoadFile("HackGULastRecodeFix.yml");
yml_t yml;

/**
 * @brief All the valid game DLL's.
 *
 */
std::vector<std::string> gameDllTable = {
    "hackGU_terminal.dll",
    "hackGU_title.dll",
    "hackGU_vol1.dll",
    "hackGU_vol2.dll",
    "hackGU_vol3.dll",
    "hackGU_vol4.dll",
};

/**
 * @brief Initializes logging for the application.
 *
 * This function performs the following tasks:
 * 1. Initializes the spdlog logging library and sets up a file logger.
 * 2. Retrieves and logs the path and name of the executable module.
 * 3. Logs detailed information about the module to aid in debugging.
 *
 * @return void
 */
void logInit() {
    // spdlog initialisation
    auto logger = spdlog::basic_logger_mt("HackGULastRecodeFix", "HackGULastRecodeFix.log", true);
    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::debug);

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    std::filesystem::path exeFilePath = exePath;
    std::string exeName = exeFilePath.filename().string();

    // Log module details
    LOG("-------------------------------------");
    LOG("Compiler: {:s}", Utils::getCompilerInfo().c_str());
    LOG("Compiled: {:s} at {:s}", __DATE__, __TIME__);
    LOG("Version: {:s}", VERSION);
    LOG("Module Name: {:s}", exeName.c_str());
    LOG("Module Path: {:s}", exeFilePath.string().c_str());
    LOG("Module Addr: 0x{:x}", (uintptr_t)baseModule);
}

/**
 * @brief Reads and parses configuration settings from a YAML file.
 *
 * This function performs the following tasks:
 * 1. Reads general settings from the configuration file and assigns them to the `yml` structure.
 * 2. Initializes global settings if certain values are missing or default.
 * 3. Logs the parsed configuration values for debugging purposes.
 *
 * @return void
 */
void readYml() {
    yml.name = config["name"].as<std::string>();

    yml.masterEnable = config["masterEnable"].as<bool>();

    yml.resolution.width = config["resolution"]["width"].as<int>();
    yml.resolution.height = config["resolution"]["height"].as<int>();

    yml.feature.combatOverlay.enable = config["features"]["combatOverlay"]["enable"].as<bool>();

    if (yml.resolution.width == 0 || yml.resolution.height == 0) {
        std::pair<int, int> dimensions = Utils::GetDesktopDimensions();
        yml.resolution.width  = dimensions.first;
        yml.resolution.height = dimensions.second;
    }
    yml.resolution.aspectRatio = (float)yml.resolution.width / (float)yml.resolution.height;
    normalizedWidth = (16.0f / 9.0f) * (float)yml.resolution.height;
    normalizedOffset = (float)(yml.resolution.width - normalizedWidth) / 2.0f;
    widthScalingFactor = (float)yml.resolution.width / (float)normalizedWidth;

    LOG("Name: {}", yml.name);
    LOG("MasterEnable: {}", yml.masterEnable);
    LOG("Resolution.Width: {}", yml.resolution.width);
    LOG("Resolution.Height: {}", yml.resolution.height);
    LOG("Resolution.AspectRatio: {}", yml.resolution.aspectRatio);
    LOG("Feature.CombatOverlay.Enable: {}", yml.feature.combatOverlay.enable);
    LOG("Normalized Width: {}", normalizedWidth);
    LOG("Normalized Offset: {}", normalizedOffset);
    LOG("Width Scaling Factor: {}", widthScalingFactor);
}

/**
 * @brief Centers the UI of the game to 16:9 aspect ratio.
 *
 * This function performs the following tasks:
 * 1. Checks if the master enable is enabled based on the configuration.
 * 2. Searches for a specific memory pattern in the base module.
 * 3. Hooks at the identified pattern to change game logic.
 *
 * @details
 * The function uses a pattern scan to find a specific byte sequence in the memory of the base module.
 * If the pattern is found, it is hooked and the game logic is modified.
 *
 * How was this found?
 * This was a huge rabbit hole... It first started by accidently modifying some memory which had 0x3F80_0000
 * which when modified would squish the entire game inwards. This discovery made me dig much deeper into
 * what parts of the game were reading this memory location and eventually I stepped into the game code
 * which deals with the UI.
 *
 * Through some more digging I eventually got to some float operations which through some experimentation
 * found out that if we multiply the width here it was essentially being used as a scaler values in
 * other parts of the code to calculate actual placements of UI elements.
 *
 * @return void
 */
void centerUiFix() {
    const char* patternFind = "C7 87 ?? ?? ?? ?? ?? ?? ?? ??    F3 41 0F 5C C1";
    uintptr_t  hookOffset = 0;

    bool enable = yml.masterEnable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        if (addr.size() > 0) {
            uint8_t* hit = (uint8_t*)addr[0];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            LOG("Found '{}' @ {:s}+{:x}", patternFind, strBaseModule, relAddr);
            uintptr_t hookAbsAddr = absAddr + hookOffset;
            uintptr_t hookRelAddr = relAddr + hookOffset;
            // Memory leak caused by SafetyMidHook object
            centerUiHook.push_back(
                safetyhook::create_mid(
                    reinterpret_cast<void*>(hookAbsAddr),
                    [](SafetyHookContext& ctx) {
                        ctx.xmm0.f32[0] = static_cast<float>(yml.resolution.width) * widthScalingFactor;
                    }
                )
            );
            LOG("Hooked @ {:s}+{:x}", strBaseModule, hookRelAddr);
        }
        else {
            LOG("Did not find '{}'", patternFind);
        }
    }
}

/**
 * @brief Fixes the aspect ratio by applying the correct one based on provided resolution in YAML file.
 *
 * This function performs the following tasks:
 * 1. Checks if the master enable is enabled based on the configuration.
 * 2. Searches for a specific memory pattern in the base module.
 * 3. Patches the identified pattern.
 *
 * @details
 * The function uses a pattern scan to find a specific byte sequence in the memory of the base module.
 * If the pattern is found, it is patched with a specific byte sequence.
 *
 * How was this found?
 * This is a common pattern to look for. The pattern we want to find '39 8E E3 3F' is the aspect ratio
 * for 16:9, and given 16:9 is the defacto standard as of writing this so its not surprising to find this
 * in the code as there are calculations in the game based on it.
 *
 * Changing this value to anything else, ie the hex for 32:9 or 21:9, causes the game to render at for that
 * resolution regardless of the resolution of the game window or viewport.
 *
 * @return void
 */
void aspectRatioFix() {
    const char* patternFind  = "39 8E E3 3F";
    std::string patternPatch = Utils::bytesToString(&yml.resolution.aspectRatio, sizeof(float));
    LOG("{}", patternPatch);

    bool enable = yml.masterEnable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        if (addr.size() > 0) {
            uint8_t* hit = (uint8_t*)addr[0];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            Utils::patch(absAddr, patternPatch.c_str());
            LOG("Patched '{}' with '{}' @ {:s}+{:x}", patternFind, patternPatch, strBaseModule, relAddr);
        }
        else {
            LOG("Did not find '{}'", patternFind);
        }
    }
}

/**
 * @brief Fixes the viewport and expands game rendering area to fit the screen.
 *
 * This function performs the following tasks:
 * 1. Checks if the master enable is enabled based on the configuration.
 * 2. Searches for a specific memory pattern in the base module.
 * 3. Patches the identified pattern.
 *
 * @details
 * The function uses a pattern scan to find a specific byte sequence in the memory of the base module.
 * If the pattern is found, it is patched with a specific byte sequence.
 *
 * How was this found?
 * Working with other games a common pattern is '39 8E E3 38' which is sometimes used in the assembly
 * to get the width and height of the screen in a 16:9 aspect ratio regardless of what the screen resolution
 * actually is.
 *
 * There were several hits for this value but it was this one in specific which dealt with resolution.
 * Moving down a couple of lines we encounter the line:
 *   hackGU_vol1.dll+10D6CC - 41 D1 F8              - sar r8d,1
 *
 * This instruction above takes the width held in the r8 register and divides it by 2, I can only assume
 * this the result is used as a scaler in other calculations with respect to the screen resolution. In
 * order to get the right value for all resolutions we need to inject the yml provided resolution and
 * multiply it by 2 effectively cancelling out the shift operation performed above. This will let the
 * game render the viewport up to the desired resolution provided within the yml file.
 *
 * @return void
 */
void viewportFix() {
    const char* patternFind  = "41 D1 F8    41 8B C0    C1 E8 1F";
    uintptr_t  hookOffset = 0;

    bool enable = yml.masterEnable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        if (addr.size() > 0) {
            uint8_t* hit = (uint8_t*)addr[0];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            // Utils::patch(absAddr, patternPatch);
            // LOG("Patched '{}' with '{}' @ ABS::0x{:x} REL::0x{:x}", patternFind, patternPatch, absAddr, relAddr);
            LOG("Found '{}' @ {:s}+{:x}", patternFind, strBaseModule, relAddr);
            // Memory leak caused by SafetyMidHook object
            uintptr_t hookAbsAddr = absAddr + hookOffset;
            uintptr_t hookRelAddr = relAddr + hookOffset;
            centerUiHook.push_back(
                safetyhook::create_mid(
                    reinterpret_cast<void*>(hookAbsAddr),
                    [](SafetyHookContext& ctx) {
                        ctx.r8 = yml.resolution.width * 2;
                    }
                )
            );
            LOG("Hooked @ {:s}+{:x}", strBaseModule, hookRelAddr);
        }
        else {
            LOG("Did not find '{}'", patternFind);
        }
    }
}

/**
 * @brief Corrects text bubble placement.
 *
 * This function performs the following tasks:
 * 1. Checks if the master enable is enabled based on the configuration.
 * 2. Searches for a specific memory pattern in the base module.
 * 3. Hooks at the identified pattern to inject a new value into xmm0.f32[0] when a particular
 *    value is encountered.
 *
 * @details
 * The function uses a pattern scan to find a specific byte sequence in the memory of the base module.
 * If the pattern is found, a hook is created at an offset from the found pattern address. The hook
 * injects a new value into xmm0.f32[0] when a particular value is encountered.
 *
 * How was this found?
 * This is a long and agregious rabbit hole, which started by analyzing UI element objects and
 * subobjects and while there were gains made on that front, ultimately nothing of value came
 * out of it as it proved difficult to find the control for everything that would work seemlessly.
 * And the values that would move the text bubbles around made no sense to me so I couldn't
 * construct some way of recalculating the right placement. Also the addresses of the objects
 * were dynamic so it was impossible to track which object controlled the text bubbles.
 *
 * Eventually a breakthrough was made and I realized that changing the aspect ratio that is
 * hardcoded in the exe would change the placements of the text bubbles so this was investigated.
 *
 * The aspect ratio is read repeatedly and only in one location and instantly saved it:
 *   hackGU_vol1.dll+199996 - F3 0F10 05 E6687600   - movss xmm0,[hackGU_vol1.dll+900284]
 *   hackGU_vol1.dll+19999E - F3 0F11 44 24 24      - movss [rsp+24],xmm0
 *
 * Tracing rsp+24 we get a hit that it the aspect ratio is used in a division operation, and
 * a couple of lines of assembly later saved:
 *   hackGU_vol1.dll+18432F - F3 0F5E 4B 04         - divss xmm1,[rbx+04]
 *   hackGU_vol1.dll+18435C - F3 0F11 0F            - movss [rdi],xmm1
 *
 * This is where it gets tricky as this part of the game code is used by a lot of other stuff,
 * so rbx+04 would not always have the aspect ratio that was saved earlier, the exact same applies
 * for rdi. the RBX and RDI registers stored various addresses for objects and subobjects respectively
 * for various parts of the game that we are not concerned with. We needed to filter out addresses
 * whenever rbx+04 was not the aspect ratio and for RDI, well we needed to test every single unique
 * address possible. Eventually we found the address in RDI that was for the text bubbles. But before
 * we get there lets talk the math behind what is saved in RDI when rbx+04 is the aspect ratio.
 *
 * Whenever the aspect ratio was in rbx+04 xmm1 already had 2.41f. The significance of this value is
 * unknown, but it is a key value in getting this fix to work. The game performs the operation:
 * 2.41f / aspect ratio, if aspect ratio is 16:9 then the division operation would result in 1.35f.
 *
 * Tracing RDI gets several hits to various parts of the game code, but only one of these hits is
 * directly linked to text bubble placement and that is:
 * hackGU_vol1.dll+13ADA1 - F3 41 0F10 00         - movss xmm0,[r8]
 *
 * This line once again controls a lot of stuff in the game and if it messed up then the entire
 * game will glitch out. So here we needed to check to make sure that R8 is the value that you
 * would get from division operation performed earlier, 2.41f / aspect ratio, only when R8 has
 * this value can it be safely modified.
 *
 * The value that you would inject here into xmm0 is, 2.41f / 1.87f = 1.35f, where 1.87f is the
 * float representation for 16:9.
 *
 * The reason this works is because the UI itself is forcefully squished back to 16:9 by another
 * fix here as to not be stretched and that applies to everything UI related including text bubble
 * locations, leading to an offset issue. By forcing a tighter aspect ratio in this location we
 * are basically undoing the offset introduced as the game thinks that rendering is 16:9 and so
 * scales the text bubbles appropriately to where they should be, above NPC heads.
 *
 * In the code below we dont take the chance of doing a raw comparision we calculate exactly
 * what both values should be as to not throw off anything and garentee a comparison correctly.
 *
 * @return void
 *
 * @note As an added bonus this fix also seems to have fixed the drift that the ingame cursor
 * experiences when you approach NPCs to interact with them.
 */
void textBubblePlacementFix() {
    const char* patternFind0  = "F3 0F 5E 4B 04    48 89 47 04";
    const char* patternFind1  = "F3 41 0F 10 48 08    0F C6 C0 00";
    uintptr_t  hookOffset = 0;
    static float scaler = 0;

    bool enable = yml.masterEnable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind0, &addr);
        if (addr.size() > 0) {
            uint8_t* hit = (uint8_t*)addr[0];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            LOG("Found '{}' @ {:s}+{:x}", patternFind0, strBaseModule, relAddr);
            // Memory leak caused by SafetyMidHook object
            uintptr_t hookAbsAddr = absAddr + hookOffset;
            uintptr_t hookRelAddr = relAddr + hookOffset;
            centerUiHook.push_back(
                safetyhook::create_mid(
                    reinterpret_cast<void*>(hookAbsAddr),
                    [](SafetyHookContext& ctx) {
                        if (*(float*)(ctx.rbx + 0x4) == yml.resolution.aspectRatio) {
                            scaler = ctx.xmm1.f32[0];
                        }
                    }
                )
            );
            LOG("Hooked @ {:s}+{:x}", strBaseModule, hookRelAddr);
        }
        else {
            LOG("Did not find '{}'", patternFind0);
        }
        addr.clear();
        Utils::patternScan(baseModule, patternFind1, &addr);
        if (addr.size() > 0) {
            uint8_t* hit = (uint8_t*)addr[0];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            LOG("Found '{}' @ {:s}+{:x}", patternFind1, strBaseModule, relAddr);
            // Memory leak caused by SafetyMidHook object
            uintptr_t hookAbsAddr = absAddr + hookOffset;
            uintptr_t hookRelAddr = relAddr + hookOffset;
            centerUiHook.push_back(
                safetyhook::create_mid(
                    reinterpret_cast<void*>(hookAbsAddr),
                    [](SafetyHookContext& ctx) {
                        float gameCalculatedScaler = scaler / yml.resolution.aspectRatio;
                        if (ctx.xmm0.f32[0] == gameCalculatedScaler) {
                            ctx.xmm0.f32[0] = scaler / std::bit_cast<float>(0x3FE38E39);
                        }
                    }
                )
            );
            LOG("Hooked @ {:s}+{:x}", strBaseModule, hookRelAddr);
        }
        else {
            LOG("Did not find '{}'", patternFind1);
        }
    }
}

/**
 * @brief Fixes combat overlay.
 *
 * This function performs the following tasks:
 * 1. Checks if the master enable is enabled based on the configuration.
 * 2. Searches for a specific memory pattern in the base module.
 * 3. Hooks at the identified pattern to modify game logic.
 *
 * @details
 * The function uses a pattern scan to find a specific byte sequence in the memory of the base module.
 * The hook injects new game logic.
 *
 * How was this found?
 * Due to the center UI fix the combat overlay is affected which causes it to shrink back to 16:9,
 * which at larger aspect ratios we do not want as the overlay does not cover the whole screen and
 * leads to a degradated experience.
 *
 * This fix hooks exactly where the main object copies its contents into the subobject which is the
 * combat overlay I think. The game makes it very hard to distinguish between objects at a top level
 * with changing addresses, no ID number to identify, etc., but here there is what I think is an ID
 * number that can be used to identify the object and this ID only works for this object, there are
 * other ID numbers that is shared among many objects, making it very difficult if not impossible
 * to differentiate what object is what.
 *
 * In register r13 I believe the ID of the object is stored which has worked flawlessly as an
 * identifier. So within the hook we check we check to make sure that r13 is 0x68 which is the ID
 * and just for safety check r14 is 0 which also stays constant and another layer of security that
 * we have the right object context.ABC
 *
 * From there we can then make the necessary changes to the combat overlay and overwrite the contents
 * at rdx+280 and rdx+2B0. What is nice is that the calculation is simple for the replacement values
 * and works at all aspect ratios.
 *
 * @return void
 */
void combatOverlayFix() {
    const char* patternFind  = "8B 82 80 02 00 00    4C 8D 89 E0 00 00 00";
    uintptr_t  hookOffset = 0;

    bool enable = yml.masterEnable && yml.feature.combatOverlay.enable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        if (addr.size() > 0) {
            uint8_t* hit = (uint8_t*)addr[0];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            LOG("Found '{}' @ {:s}+{:x}", patternFind, strBaseModule, relAddr);
            // Memory leak caused by SafetyMidHook object
            uintptr_t hookAbsAddr = absAddr + hookOffset;
            uintptr_t hookRelAddr = relAddr + hookOffset;
            centerUiHook.push_back(
                safetyhook::create_mid(
                    reinterpret_cast<void*>(hookAbsAddr),
                    [](SafetyHookContext& ctx) {
                        if (ctx.r13 == 0x68 && ctx.r14 == 0) {
                            if (yml.feature.combatOverlay.enable == true) {
                                *(uint32_t*)(ctx.rdx + 0x280) = 1.0f / ((float)yml.resolution.width / 2.0f);
                                *(uint32_t*)(ctx.rdx + 0x2B0) = 0xBF800000;
                            }
                            else {
                                *(uint32_t*)(ctx.rdx + 0x280) = 0;
                            }
                        }
                    }
                )
            );
            LOG("Hooked @ {:s}+{:x}", strBaseModule, hookRelAddr);
        }
        else {
            LOG("Did not find '{}'", patternFind);
        }
    }
}

/**
 * @brief Fixes UI elements.
 *
 * This function performs the following tasks:
 * 1. Checks if the master enable is enabled based on the configuration.
 * 2. Searches for a specific memory pattern in the base module.
 * 3. Hooks at the identified pattern to modify game logic.
 *
 * @details
 * The function uses a pattern scan to find a specific byte sequence in the memory of the base module.
 * The hook injects new game logic.
 *
 * How was this found?
 * This is a build off of `centerUiFix`. The consequence of centering the UI is that this does not modify
 * the coordinates of where and how much of the UI elements should be shown, THIS IS NOT PLACEMENT RELATED.
 *
 * The challenge here is that it is very difficult if not impossoble to actually seperate all the objects part
 * of the UI or just the game in general. The addresses of the objects are all dynamic and change from game
 * boot to boot, the ID well at least what I think are IDs are also for some reason different everytime you
 * play, and the objects that do get generated is based on the game state, ie if your starting a new game
 * and the ingame cutscene goes off it throws off when UI elements are dynamically allocated so going by index
 * is not a viable solution to figure out what object is what, and loading a new area also diversifies the
 * addresses and it is a complete pain to work with let alone might be impossible to actually everything.
 *
 * Anyway... there are two things we do in the hook, the first is calculate the correct offsets for the map
 * and the other thing is hard code width to the desired resolution.
 *
 * So first background on four values:
 * rbx+388 - X offset of where UI rendering should start for a component.
 * rbx+390 - Y offset of where UI rendering should start for a component.
 * rbx+394 - How many pixels of the UI element to render, X wise.
 * rbx+398 - How many pixels of the UI element to render, Y wise.
 *
 * This is primarily an ultrawide patch to enable 21:9 and 32:9, so the any Y based (height resolution) calculations
 * will remain intact. The X however needs to change.
 *
 * In the hook as well we do some additional calculations `mapOffset` and `mapOffsetCorrected`. The `mapOffset`
 * calculation is what the game calculates which we recalculate exactly as the game does it so that we have a good
 * and exact value for comparison. The `mapOffsetCorrected` is the same as `mapOffset` but with a correction applied
 * using the width of the screen if it was 16:9.
 *
 * So now the if statements, we really only need to worry about the map everything else we can hardcode 0 and width
 * resolution respectively.
 *
 * So the ingame map, it is made of three components, the actual map, the shadow around the map, and the map background.
 * The reason we need this calculation for the map is because the game actually places the entire map and uses the 4
 * memory values above to determine what to show and what remains hidden. The range is a bit weird and this is because
 * depending where you are in the game for whatever reason this calculation changes, because the 40.0f changes by a
 * little bit and kills this fix so we need a range that just works and thankfully no UI component, that I encountered
 * thus far, fits in the defined range in the hook.
 *
 * The last part if the else this actually fixes other issues, if we let the game do its own thing text is broken and
 * cutoff early or late depending where on the screen it is located, certain menus are also broken, so by hardcoding
 * the offset to 0 and amount of pixels to render to the full width we fix all broken cases. This works as the UI
 * components for other parts dont have the same issue as the map, where you only want a portion of it to show, the
 * other UI parts can be FULL display hence we can render the entire thing without worry by hardcoding.
 *
 * @return void
 */
void uiElementsFix() {
    const char* patternFind  = "48 8B 74 24 38    48 8B 5C 24 40    48 83 C4 20    5F    C3    48 8D 81 88 03 00 00";
    uintptr_t  hookOffset = 0;

    bool enable = yml.masterEnable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        if (addr.size() > 0) {
            uint8_t* hit = (uint8_t*)addr[0];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            LOG("Found '{}' @ {:s}+{:x}", patternFind, strBaseModule, relAddr);
            // Memory leak caused by SafetyMidHook object
            uintptr_t hookAbsAddr = absAddr + hookOffset;
            uintptr_t hookRelAddr = relAddr + hookOffset;
            centerUiHook.push_back(
                safetyhook::create_mid(
                    reinterpret_cast<void*>(hookAbsAddr),
                    [](SafetyHookContext& ctx) {
                        uint32_t mapOffset0 = static_cast<uint32_t>((yml.resolution.width / 682.0f) * 40.0f + 0.5f);
                        uint32_t mapOffset1 = static_cast<uint32_t>((yml.resolution.width / 682.0f) * std::bit_cast<float>(0x4227799a) + 0.5f);
                        uint32_t mapOffsetCorrected = static_cast<uint32_t>((static_cast<float>(normalizedWidth) / 682.0f) * 40.0f + 0.5f);
                        if (mapOffset0 == *(uint32_t*)(ctx.rbx + 0x388) || mapOffset1 == *(uint32_t*)(ctx.rbx + 0x388)) {
                            //LOG("{:x}", ctx.rbx);
                            *(uint32_t*)(ctx.rbx + 0x388) = normalizedOffset + mapOffsetCorrected;
                            *(uint32_t*)(ctx.rbx + 0x390) = *(uint32_t*)(ctx.rbx + 0x394);
                        }
                        else {
                            *(uint32_t*)(ctx.rbx + 0x388) = 0;
                            *(uint32_t*)(ctx.rbx + 0x390) = yml.resolution.width;
                        }
                    }
                )
            );
            LOG("Hooked @ {:s}+{:x}", strBaseModule, hookRelAddr);
        }
        else {
            LOG("Did not find '{}'", patternFind);
        }
    }
}

/**
 * @brief Fixes aspect ratio for cutscenes.
 *
 * This function performs the following tasks:
 * 1. Checks if the master enable is enabled based on the configuration.
 * 2. Searches for a specific memory pattern in the base module.
 * 3. Hooks at the identified pattern to modify game logic.
 *
 * @details
 * The function uses a pattern scan to find a specific byte sequence in the memory of the base module.
 * The hook injects new game logic.
 *
 * How was this found?
 * The cutscene fix is interesting because the fix is in the same function as where the aspect ratio is
 * read. In the assembly of the game this function is guarded with an if statement, where if the object
 * going into the object is not a cutscene object then execute code that uses the aspect ratio value else
 * jump away from that logic and go into the logic which is used when the object is a cutscene object.
 *
 * The entire assembly for determining the aspect ratio to render is rather short, but there are four
 * memory writes to make watch:
 * 1 - hackGU_vol1.dll+199A10 - F3 0F11 44 24 38      - movss [rsp+38],xmm0
 * 2 - hackGU_vol1.dll+199A21 - F3 0F11 4C 24 3C      - movss [rsp+3C],xmm1
 * 3 - hackGU_vol1.dll+199A39 - F3 0F11 44 24 30      - movss [rsp+30],xmm0
 * 4 - hackGU_vol1.dll+199A3F - F3 0F11 4C 24 34      - movss [rsp+34],xmm1
 *
 * By default the game will load 8.0f into xmm registers which will then be multiplied by some multipliers
 * that I don't understand, but thats not important here. So the memory writes above:
 * 1. rsp+38 stores -4.84f (0xC09AEF7D) where as the number approaches +infinity it will shift the image to
 * the right and render at a tighter aspect ratio, the opposite happens as the number approaches -infinity.
 * 2. rsp+3C stores 4.84f (0x409AEF7D) which will do the opposite of 1. where as +infinity is approached the
 * image shifts to the left and render at a tighter aspect ratio and the opposite happens as the number
 * approaches -infinity.
 * 3. rsp+30 stores 3.13f (0x404872A3) which does the same as 2. but on the y axis and affecting the top.
 * 4. rsp+34 stores -2.31f (0xC0142837) which does the same as 1. but on the y axis and affecting the bottom.
 *
 * What is important is that we do not touch what happens on the y axis so the writes to rsp+30 and rsp+34
 * stay as they are. So when getting things to render at the correct aspect ration for our screen resolution
 * or whatever was provided in the yml file. These numbers never change regardless of resolution and these
 * numbers are purely for 16:9 aspect ratio. We can easily adapt this for any aspect ratio we want by
 * figuring out a scaling factor for the desired resolution, basically how much more screen space as a
 * percentage is ie. 21:9 than 16:9, which is 31.25% (21/16) more screen space and multiplying the values of
 * rsp+38 and rsp+3C by that scaling factor. And just like that we get in engine cutscenes to render at the
 * desired aspect ratio.
 *
 * @return void
 */
void cutsceneFix() {
    const char* patternFind  = "0F 28 CA    F3 0F 59 89 A4 03 00 00";
    uintptr_t  hookOffset = 0;

    bool enable = yml.masterEnable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        if (addr.size() > 0) {
            uint8_t* hit = (uint8_t*)addr[0];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            LOG("Found '{}' @ {:s}+{:x}", patternFind, strBaseModule, relAddr);
            // Memory leak caused by SafetyMidHook object
            uintptr_t hookAbsAddr = absAddr + hookOffset;
            uintptr_t hookRelAddr = relAddr + hookOffset;
            centerUiHook.push_back(
                safetyhook::create_mid(
                    reinterpret_cast<void*>(hookAbsAddr),
                    [](SafetyHookContext& ctx) {
                        *(float*)(ctx.rsp + 0x38) = *(float*)(ctx.rsp + 0x38) * widthScalingFactor;
                        *(float*)(ctx.rsp + 0x3C) = *(float*)(ctx.rsp + 0x3C) * widthScalingFactor;
                    }
                )
            );
            LOG("Hooked @ {:s}+{:x}", strBaseModule, hookRelAddr);
        }
        else {
            LOG("Did not find '{}'", patternFind);
        }
    }
}

/**
 * @brief Fixes aspect ratio for cutscenes.
 *
 * This function performs the following tasks:
 * 1. Checks if the master enable is enabled based on the configuration.
 * 2. Searches for a specific memory pattern in the base module.
 * 3. Hooks at the identified pattern to modify game logic.
 *
 * @details
 * The function uses a pattern scan to find a specific byte sequence in the memory of the base module.
 * The hook injects new game logic.
 *
 * How was this found?
 * The anti-aliasing fix was relatively simple to find. We can use the assumption that the code for
 * the game graphics settings in general are all in tight proximity to each other. At first analysis
 * I tried to see if the settings are encoded as strings and quickly found out they are not. But the
 * one thing I did already know is that the resolution strings are encoded as decimal values within
 * the code. Based on this info I tracked down the game uses a table that stores the values for
 * resolution and passes in an index to the table to get the value.
 *
 * Getting out of the function where the table indexing happens leads us into a function where a lot
 * of Windows API game window calls are made, and this is where the anti-aliasing value is set, so from
 * here we are onto something. Going backwards a little bit and setting a bunch of breakpoints on a
 * bunch of conditional jumps and playing around with the anti-aliasing value we can see that the
 * setting that in fact also traverse this code. A couple breakpoints later and we are sitting at
 * the code guarded by a conditional jump that only gets triggered by a change in the anti-aliasing
 * value:
 * hackGU_title.dll+10BFF4 - 44 8989 B00B0000 - mov [rcx+BB0],r9
 *
 * Register r9 can have the following values for anti-aliasing:
 * 1 - LOW
 * 2 - MEDIUM
 * 3 - HIGH
 *
 * The mod for whatever reason breaks when anti-aliasing is set to HIGH, so we need to make sure that
 * high cannot be acheived, so when a value greater than 2 is detected we need to set it to 2. Yes,
 * with this method we are not allowing the game to use the highest setting, which means reduced
 * visual fidelity, but this is a trade off we have to make in order to get the mod to work. As I am
 * a bit too lazy to figure out how and where the game uses the HIGH setting that causes this break.
 *
 * @return void
 */
void constrainAntiAliasing() {
    const char* patternFind  = "44 0F BE 4A 10    44 0F BE 52 11";
    uintptr_t  hookOffset = 0;

    bool enable = yml.masterEnable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    if (enable) {
        std::vector<uint64_t> addr;
        Utils::patternScan(baseModule, patternFind, &addr);
        if (addr.size() > 0) {
            uint8_t* hit = (uint8_t*)addr[0];
            uintptr_t absAddr = (uintptr_t)hit;
            uintptr_t relAddr = (uintptr_t)hit - (uintptr_t)baseModule;
            LOG("Found '{}' @ {:s}+{:x}", patternFind, strBaseModule, relAddr);
            // Memory leak caused by SafetyMidHook object
            uintptr_t hookAbsAddr = absAddr + hookOffset;
            uintptr_t hookRelAddr = relAddr + hookOffset;
            centerUiHook.push_back(
                safetyhook::create_mid(
                    reinterpret_cast<void*>(hookAbsAddr),
                    [](SafetyHookContext& ctx) {
                        uint8_t antiAliasingVal = *(uint8_t*)(ctx.rdx + 0x10);
                        if (antiAliasingVal > 0x2) {
                            *(uint8_t*)(ctx.rdx + 0x10) = 0x2;
                        }
                    }
                )
            );
            LOG("Hooked @ {:s}+{:x}", strBaseModule, hookRelAddr);
        }
        else {
            LOG("Did not find '{}'", patternFind);
        }
    }
}

/**
 * @brief Wait for a game DLL from the `gameDllTable` to load.
 *
 * @return void
 */
void waitForGameDllLoad() {
    // Get the current process handle
    HANDLE hProcess = GetCurrentProcess();

    // Array to store module handles
    HMODULE hMods[1024];
    DWORD cbNeeded;

    // Get the list of modules in the process
    while(1) {
        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
            size_t moduleCount = cbNeeded / sizeof(HMODULE);
            for (size_t i = 0; i < moduleCount; ++i) {
                char moduleName[MAX_PATH];
                if (GetModuleFileNameA(hMods[i], moduleName, sizeof(moduleName))) {
                    std::string strModuleName(moduleName);
                    for (const auto& strTargetModuleName : gameDllTable) {
                        if (strModuleName.find(strTargetModuleName) != std::string::npos) {
                            LOG("{} Loaded", strTargetModuleName);
                            baseModule = GetModuleHandle(strTargetModuleName.c_str());
                            strBaseModule = strTargetModuleName;
                            return;
                        }
                    }
                }
            }
        } else {
            LOG("Failed to enumerate modules.");
        }
    }
}

/**
 * @brief Wait for the current game DLL to unload.
 *
 * @return void
 */
void waitForGameDllUnload() {
    while(1) {
        HMODULE hModule = GetModuleHandle(strBaseModule.c_str());
        if (hModule == NULL) {
            LOG("{} Dropped", strBaseModule);
            return;
        }
    }
}

/**
 * @brief This function serves as the entry point for the DLL. It performs the following tasks:
 * 1. Initializes the logging system.
 * 2. Reads the configuration from a YAML file.
 * 3. Applies a center UI fix.
 *
 * @param lpParameter Unused parameter.
 * @return Always returns TRUE to indicate successful execution.
 */
DWORD __stdcall Main(void* lpParameter) {
    logInit();
    readYml();
    while(1) {
        waitForGameDllLoad();
        constrainAntiAliasing();
        viewportFix();
        aspectRatioFix();
        centerUiFix();
        uiElementsFix();
        combatOverlayFix();
        textBubblePlacementFix();
        cutsceneFix();
        waitForGameDllUnload();
    }
    return true;
}

/**
 * @brief Entry point for a DLL, called by the system when the DLL is loaded or unloaded.
 *
 * This function handles various events related to the DLL's lifetime and performs actions
 * based on the reason for the call. Specifically, it creates a new thread when the DLL is
 * attached to a process.
 *
 * @details
 * The `DllMain` function is called by the system when the DLL is loaded or unloaded. It handles
 * different reasons for the call specified by `ul_reason_for_call`. In this implementation:
 *
 * - **DLL_PROCESS_ATTACH**: When the DLL is loaded into the address space of a process, it
 *   creates a new thread to run the `Main` function. The thread priority is set to the highest,
 *   and the thread handle is closed after creation.
 *
 * - **DLL_THREAD_ATTACH**: Called when a new thread is created in the process. No action is taken
 *   in this implementation.
 *
 * - **DLL_THREAD_DETACH**: Called when a thread exits cleanly. No action is taken in this implementation.
 *
 * - **DLL_PROCESS_DETACH**: Called when the DLL is unloaded from the address space of a process.
 *   No action is taken in this implementation.
 *
 * @param hModule Handle to the DLL module. This parameter is used to identify the DLL.
 * @param ul_reason_for_call Indicates the reason for the call (e.g., process attach, thread attach).
 * @param lpReserved Reserved for future use. This parameter is typically NULL.
 * @return BOOL Always returns TRUE to indicate successful execution.
 */
BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    HANDLE mainHandle;
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
            CloseHandle(mainHandle);
        }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
