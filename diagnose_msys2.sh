#!/usr/bin/env bash
set -u

echo "=== AquaVillage3D Cinematic / MSYS2 diagnostics ==="
echo "MSYSTEM=${MSYSTEM:-unset}"
echo "PATH=$PATH"
echo

for tool in g++ cmake ninja pkg-config; do
  if command -v "$tool" >/dev/null 2>&1; then
    echo "[OK] $tool -> $(command -v "$tool")"
  else
    echo "[MISSING] $tool"
  fi
done

echo
if command -v g++ >/dev/null; then g++ --version | head -n 1; fi
if command -v cmake >/dev/null; then cmake --version | head -n 1; fi
if command -v ninja >/dev/null; then ninja --version | head -n 1; fi

echo
if command -v pkg-config >/dev/null; then
  pkg-config --modversion glfw3 2>/dev/null && echo "[OK] glfw3 pkg-config" || echo "[MISSING] glfw3 pkg-config"
  pkg-config --modversion glew 2>/dev/null && echo "[OK] glew pkg-config" || echo "[MISSING] glew pkg-config"
fi

if [[ -f /ucrt64/include/glm/glm.hpp ]]; then
  echo "[OK] GLM header: /ucrt64/include/glm/glm.hpp"
else
  echo "[MISSING] /ucrt64/include/glm/glm.hpp"
fi

echo
if [[ "${MSYSTEM:-}" == "UCRT64" ]]; then
  echo "Environment looks correct: UCRT64"
else
  echo "WARNING: Start the project from the MSYS2 UCRT64 terminal."
fi
