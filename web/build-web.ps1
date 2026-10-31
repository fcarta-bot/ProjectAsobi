# Compila la versione per browser / iPad nella cartella build-web.
# Serve Emscripten installato in C:\Users\flavi\emsdk (vedi "Versione iPad" nel README).
# Uso, da PowerShell nella cartella del progetto:   .\web\build-web.ps1
param([string]$Emsdk = "C:\Users\flavi\emsdk")

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot
$clion = "C:\Users\flavi\AppData\Local\Programs\CLion 2\bin"

# Ambiente di Emscripten (emcc, node, python) + CMake e Ninja di CLion
. "$Emsdk\emsdk_env.ps1" | Out-Null
$env:PATH = "$clion\cmake\win\x64\bin;$clion\ninja\win\x64;" + $env:PATH

$toolchain = "$Emsdk\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake"
cmake -S $root -B "$root\build-web" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
cmake --build "$root\build-web" --target ProjectAsobi

Write-Host ""
Write-Host "Fatto: build-web\index.html, index.js, index.wasm (questi tre file vanno sul sito)"
