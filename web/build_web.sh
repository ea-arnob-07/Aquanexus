#!/usr/bin/env bash
set -euo pipefail

# Requires an activated Emscripten SDK and GLM headers visible to CMake.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "$script_dir/.." && pwd)"
build_dir="$project_dir/build-web"
portal_dir="$project_dir/portal"

emcmake cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$build_dir" --parallel

mkdir -p "$portal_dir"
for artifact in "$build_dir"/bin/AquaVillage3D_Cinematic.*; do
  [[ -f "$artifact" ]] && cp -f "$artifact" "$portal_dir/"
done

if [[ -f "$script_dir/coi-serviceworker.js" ]]; then
  cp -f "$script_dir/coi-serviceworker.js" "$portal_dir/"
fi

# Rename the generated HTML to index.html so GitHub Pages serves it directly
if [[ -f "$portal_dir/AquaVillage3D_Cinematic.html" ]]; then
  mv "$portal_dir/AquaVillage3D_Cinematic.html" "$portal_dir/index.html"
fi

echo "Web simulation built and copied to: $portal_dir"
echo "Next: ./web/run_portal.sh"
