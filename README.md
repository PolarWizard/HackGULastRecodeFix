# Hack GU Last Recode Fix
![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/PolarWizard/HackGULastRecodeFix/total)

Adds support for ultrawide resolutions and additional features.

***This project is designed exclusively for Windows due to its reliance on Windows-specific APIs. The build process requires the use of PowerShell.***

## Features
- Support for ultrawide resolutions
- Enable or disable game applied in-combat gray tint overlay

## Build and Install
### Using CMake
1. Build and install:
```ps1
git clone --recurse-submodules https://github.com/PolarWizard/HackGULastRecodeFix.git
cd HackGULastRecodeFix; mkdir build; cd build
cmake  ..
cmake --build .
cmake --install .
```
`cmake ..` will attempt to find the game folder in `C:/Program Files (x86)/Steam/steamapps/common/`. If the game folder cannot be found rerun the command providing the path to the game folder:<br>`cmake .. -DGAME_FOLDER="<FULL-PATH-TO-GAME-FOLDER>"`

2. Download [d3d11.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) Win64 version
3. Extract to `hackGU`

### Using Release
1. Download and follow instructions in [latest release](https://github.com/PolarWizard/HackGULastRecodeFix/releases)

## Configuration
- Adjust settings in `hackGU/scripts/HackGULastRecodeFix.yml`

## Screenshots
| ![Demo](images/HackGULastRecodeFix_1.gif) |
| :-: |
| Fix disabled → Fix enabled |

## License
Distributed under the MIT License. See [LICENSE](LICENSE) for more information.

## External Tools

### Python
- [PyYAML](https://github.com/yaml/pyyaml)
- [screeninfo](https://github.com/rr-/screeninfo)
- [PyInstaller](https://github.com/pyinstaller/pyinstaller)

### C/C++
- [safetyhook](https://github.com/cursey/safetyhook)
- [spdlog](https://github.com/gabime/spdlog)
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
- [zydis](https://github.com/zyantific/zydis)
