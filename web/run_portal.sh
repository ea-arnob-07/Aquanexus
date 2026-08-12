#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "$script_dir/.." && pwd)"

if command -v python3 >/dev/null 2>&1; then
  python3 -m http.server 8080 --directory "$project_dir/portal"
elif command -v python >/dev/null 2>&1; then
  python -m http.server 8080 --directory "$project_dir/portal"
else
  echo "Python is required to serve the WebAssembly portal locally."
  exit 1
fi
