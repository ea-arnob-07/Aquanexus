#!/usr/bin/env bash
set -euo pipefail

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
  echo "ERROR: Open the 'MSYS2 UCRT64' terminal, not MSYS/MINGW64/CLANG64."
  echo "Current MSYSTEM=${MSYSTEM:-unknown}"
  exit 1
fi

command -v cmake >/dev/null || { echo "cmake not found"; exit 1; }
command -v ninja >/dev/null || { echo "ninja not found"; exit 1; }
command -v g++ >/dev/null || { echo "g++ not found"; exit 1; }

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

echo
echo "Build complete."
echo "Run with: ./build/bin/AquaVillage3D_Cinematic.exe"
