<#
MIT License

Copyright (c) 2025 Dominik Protasewicz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
#>

function Show-Usage {
    Write-Output "Usage: .\install.ps1 <path to 'hackGU' game folder>"
    exit 1
}

if (-not $args[0]) {
    Show-Usage
}
if (-not (Test-Path -Path $args[0])) {
    Write-Output "Error: The path '$filePath' does not exist or is not valid."
    Show-Usage
}

$gameFolder = $args[0]
$gameSubFolder = ""
$scriptsFolder = "scripts"
$fullPath = "$gameFolder\$gameSubFolder\$scriptsFolder"

$fixName = "HackGULastRecodeFix"
$pyPatch = "HackGULastRecodePatch.py"

$ymlFileContent = @"
name: Hack GU Last Recode Fix

# Enables or disables this mod
masterEnable: true

# Enter desired resolution.
# A value of 0 in either width or height will use your desktop's resolution.
resolution:
  width: 0
  height: 0

# Available features
features:

  # Game defualt behavior: If enabled combat will render gray tint overlay
  # If disabled combat will not render gray tint overlay
  combatOverlay:
    enable: true
"@

if (Test-Path -Path $gameFolder) {
    $dllPath = Get-ChildItem -Path "$PSScriptRoot\bin\*.dll" -Recurse
    Write-Output "Found DLL at $dllPath"
    New-Item -Path "$fullPath" -ItemType Directory -Force | Out-Null
    Write-Output "Copying DLL to $fullPath"
    Copy-Item -Path $dllPath -Destination "$fullPath"
    Move-Item -Path $fullPath\$fixName.dll -Destination $fullPath\$fixName.asi -Force
    Write-Output "Copying $pyPatch to $fullPath"
    Copy-Item -Path $PSScriptRoot\$pyPatch -Destination "$fullPath"
    Write-Host "Running PyInstaller..."
    pyinstaller --specpath $fullPath --distpath $fullPath --onefile $fullPath\$pyPatch
    Write-Output "Creating $fixName.yml at $fullPath"
    New-Item -Path $fullPath -Name "$fixName.yml" -ItemType File -Value $ymlFileContent -Force | Out-Null
    Write-Output "Done!"
} else {
    Write-Output "Game folder not found at $gameFolder"
}
