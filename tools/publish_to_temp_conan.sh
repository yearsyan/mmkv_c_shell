#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

remote_name="${CONAN_REMOTE_NAME:-temp-conan}"
remote_url="${CONAN_REMOTE_URL:-http://127.0.0.1:9300}"
reference="${MMKV_C_REFERENCE:-mmkv_c/2.4.1}"

export CONAN_HOME="${CONAN_HOME:-$repo_root/.conan2-temp}"

if ! conan profile path default >/dev/null 2>&1; then
  conan profile detect --force
fi

conan remote add "$remote_name" "$remote_url" -f
if [[ -n "${CONAN_USERNAME:-}" || -n "${CONAN_PASSWORD:-}" ]]; then
  if [[ -z "${CONAN_USERNAME:-}" || -z "${CONAN_PASSWORD:-}" ]]; then
    echo "Both CONAN_USERNAME and CONAN_PASSWORD are required when enabling auth" >&2
    exit 1
  fi
  conan remote login "$remote_name" "$CONAN_USERNAME" -p "$CONAN_PASSWORD"
fi

conan create . --build=missing -s build_type=Release -nr
conan upload "${reference}:*" -r "$remote_name" --check -c
