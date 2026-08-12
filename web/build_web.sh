#!/usr/bin/env bash
set -euo pipefail

# Requires an activated Emscripten SDK and GLM headers visible to CMake.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "$script_dir/.." && pwd)"
build_dir="$project_dir/build-web"
portal_sim_dir="$project_dir/portal/simulation"

emcmake cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$build_dir" --parallel

mkdir -p "$portal_sim_dir"
for artifact in "$build_dir"/bin/AquaVillage3D_Cinematic.*; do
  [[ -f "$artifact" ]] && cp -f "$artifact" "$portal_sim_dir/"
done

echo "Web simulation copied to: $portal_sim_dir"
echo "Next: ./web/run_portal.sh"
