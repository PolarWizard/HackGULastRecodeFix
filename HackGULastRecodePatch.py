# MIT License
#
# Copyright (c) 2024 Dominik Protasewicz
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

"""
Patch Tool for `hackGU.exe` executable

This script modifies a specific binary pattern in the games DLL files based on a
provided YAML configuration. The configuration specifies the desired resolution
settings, and the script computes necessary values to update the executable.

The games DLL files all feature a resolution table that the game indexes into very
early on during initialization which sets up the game window as well as various
other game calculations based off that resolution. This tool goes through each game
DLL and searches for the resolution table and replaces all entries with the new
resolution provided in the YAML file.

The reason this script is needed as opposed to be part of the DLL that is injected
is that the game exe loads and unloads various game DLL's depending on what the user
selects in the game menu. And very early on in the DLL load process the game reads
hardcoded resolution values from the DLL it reads them once and far too quickly for
the injection to modify them.

The code we replace:

Modules:
    yaml: For reading and parsing YAML configuration files.
    screeninfo: For retrieving the desktop resolution from the primary monitor.

Functions:
    read_yaml(yaml_path): Reads a YAML file and returns its content.
    get_desktop_resolution(): Retrieves the resolution of the primary desktop monitor.
    parse_signature(signature): Converts a string pattern into a byte array.
    edit_dll(game_path, search_pattern, replace_pattern): Finds and replaces binary patterns in the executable.
    patch(): Main function to read the YAML file, compute patterns, and apply the patch.

Usage:
    This script is designed for modifying hackGU_*.dll files
    This script should be placed in the scripts folder where the yml and .asi file is located and run from there,
    this tool should be a one time use case, edit the resolution portion of the yml file and run the tool.

Example:
    python patch_tool.py
"""

import yaml
from screeninfo import get_monitors

def read_yaml(yaml_path):
    """
    Reads a YAML file and returns its content as a Python object.

    Args:
        yaml_path (str): Path to the YAML file.

    Returns:
        dict or None: Parsed content of the YAML file, or None if an error occurs.
    """
    try:
        with open(yaml_path, 'r') as file:
            data = yaml.safe_load(file)  # Use safe_load to avoid potential code execution risks
        return data
    except FileNotFoundError:
        print(f"Error: File '{yaml_path}' not found.")
    except yaml.YAMLError as e:
        print(f"Error parsing YAML file: {e}")
    return None

def get_desktop_resolution() -> tuple:
    """
    Gets the resolution of the primary desktop monitor.

    Returns:
        tuple: A tuple containing the width and height of the desktop resolution
               (e.g., (1920, 1080)). If no primary monitor is found, defaults to the first monitor.
    """
    for monitor in get_monitors():
        if monitor.is_primary:
            return monitor.width, monitor.height
    # Fallback: Return the resolution of the first monitor if no primary is found
    primary_monitor = get_monitors()[0]
    return primary_monitor.width, primary_monitor.height

def parse_signature(signature) -> bytes:
    """
    Parses a string signature into a byte array, replacing wildcards with placeholders.

    Args:
        signature (str): A string representation of the binary signature, with optional
                         wildcards ("?" or "??").

    Returns:
        bytes: A byte array corresponding to the parsed signature.
    """
    byte_array = []
    for token in signature.split():
        if token == "?" or token == "??":  # Wildcard
            byte_array.append(0)  # Placeholder for wildcard
        else:
            byte_array.append(int(token, 16))  # Convert hex to int
    return bytes(byte_array)

def edit_dll(game_path, search_pattern, replace_pattern) -> int:
    """
    Searches for a binary pattern in a game dll file and replaces it with another pattern.

    Args:
        game_path (str): Path to the game dll file.
        search_pattern (str): Binary pattern to search for (in string format).
        replace_pattern (str): Binary pattern to replace with (in string format).

    Returns:
        int: Offset of the replaced pattern in the file, or None if the pattern is not found.
    """
    print(f"\nEditing: {game_path}")
    search_pattern = search_pattern.strip()
    sp1 = parse_signature(search_pattern)
    rp  = parse_signature(replace_pattern)

    try:
        with open(game_path, 'rb+') as file:
            data = file.read()
            offset = data.find(sp1, 0)
            if offset == -1:
                print(f"Cannot find: {search_pattern}")
                print("Delete and reinstall game and try again")
                return None
            print(f"Found: {search_pattern} @ 0x{hex(offset)}")
            file.seek(offset)
            file.write(rp)
            print(f"Replaced with: {replace_pattern} @ 0x{hex(offset)}")
            with open("patch.txt", "w") as file:
                file.write(replace_pattern)
    except FileNotFoundError:
        print(f"Error: File '{game_path}' not found")
    except IOError as e:
        print(f"IO Error with file: {e}")

def patch() -> int:
    """
    Reads configuration from a YAML file, computes replacement patterns, and patches the game dll.

    Returns:
        int: The result of the `edit_dll` function, or None if the YAML file is invalid.
    """
    yaml_data = read_yaml("HackGULastRecodeFix.yml")
    if yaml_data is None:
        return
    width  = yaml_data['resolution']['width']
    height = yaml_data['resolution']['height']
    print(f"YAML: resolution: width:  {yaml_data['resolution']['width']}")
    print(f"YAML: resolution: height: {yaml_data['resolution']['height']}")
    desktop_width, desktop_height = get_desktop_resolution()
    if width == 0 or height == 0:
        width, height = desktop_width, desktop_height
    offset = int((desktop_width - width) / 2)
    special = (height << 4).to_bytes(4, byteorder="little", signed=False)
    width  = width.to_bytes(4, byteorder="little", signed=False)
    height = height.to_bytes(4, byteorder="little", signed=False)
    offset = offset.to_bytes(4, byteorder="little", signed=False)
    special = " ".join(f"{byte:02X}" for byte in special)
    width = " ".join(f"{byte:02X}" for byte in width)
    height = " ".join(f"{byte:02X}" for byte in height)
    offset = " ".join(f"{byte:02X}" for byte in offset)

    search_pattern = "20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00 00 05 00 00 D0 02 00 00 00 05 00 00 20 03 00 00 00 05 00 00 00 04 00 00 50 05 00 00 00 03 00 00 A0 05 00 00 84 03 00 00 40 06 00 00 84 03 00 00 40 06 00 00 B0 04 00 00 90 06 00 00 1A 04 00 00 80 07 00 00 38 04 00 00 80 07 00 00 B0 04 00 00 00 0A 00 00 A0 05 00 00 00 0A 00 00 40 06 00 00 00 0F 00 00 70 08 00 00"
    replace_pattern = f"{width} {height} " * 15

    try:
        with open("patch.txt", 'r') as file:
            print("patch.txt found")
            data = file.readlines()
            search_pattern = data[0].strip()
            print(f"Will search for pattern: {search_pattern}")
    except FileNotFoundError:
        print("patch.txt not found")
        print(f"Will search using default pattern: {search_pattern}")
    except IOError as e:
        print(f"IO Error with file: {e}")

    edit_dll("../hackGU_terminal.dll", search_pattern, replace_pattern)
    edit_dll("../hackGU_title.dll", search_pattern, replace_pattern)
    edit_dll("../hackGU_vol1.dll", search_pattern, replace_pattern)
    edit_dll("../hackGU_vol2.dll", search_pattern, replace_pattern)
    edit_dll("../hackGU_vol3.dll", search_pattern, replace_pattern)
    edit_dll("../hackGU_vol4.dll", search_pattern, replace_pattern)

if __name__ == "__main__":
    patch()
    input("Press enter to exit...")
