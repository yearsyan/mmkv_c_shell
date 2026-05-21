#!/usr/bin/env bash
set -euo pipefail

remote_name="${CONAN_REMOTE_NAME:-private}"
reference="${MMKV_C_REFERENCE:-}"
remote_url="${CONAN_REMOTE_URL:-}"
username="${CONAN_USERNAME:-}"
password="${CONAN_PASSWORD:-}"
proxy_url="${CONAN_HTTPS_PROXY_URL:-}"
proxy_username="${CONAN_HTTPS_PROXY_USERNAME:-}"
proxy_password="${CONAN_HTTPS_PROXY_PASSWORD:-}"

if [ -z "$reference" ]; then
  echo "MMKV_C_REFERENCE is required for Conan upload" >&2
  exit 1
fi
if [ -z "$remote_url" ]; then
  echo "CONAN_REMOTE_URL secret is required for Conan upload" >&2
  exit 1
fi
if [ -z "$username" ] || [ -z "$password" ]; then
  echo "CONAN_USERNAME and CONAN_PASSWORD secrets are required for Conan upload" >&2
  exit 1
fi
if [ -z "$proxy_url" ] || [ -z "$proxy_username" ] || [ -z "$proxy_password" ]; then
  echo "CONAN_HTTPS_PROXY_URL, CONAN_HTTPS_PROXY_USERNAME, and CONAN_HTTPS_PROXY_PASSWORD secrets are required for Conan upload" >&2
  exit 1
fi

python_cmd="${PYTHON:-python}"
if ! command -v "$python_cmd" >/dev/null 2>&1; then
  python_cmd="python3"
fi

proxy_with_auth="$("$python_cmd" - <<'PY'
import os
from urllib.parse import quote, urlsplit, urlunsplit

proxy_url = os.environ["CONAN_HTTPS_PROXY_URL"]
username = quote(os.environ["CONAN_HTTPS_PROXY_USERNAME"], safe="")
password = quote(os.environ["CONAN_HTTPS_PROXY_PASSWORD"], safe="")
parts = urlsplit(proxy_url)
if not parts.scheme or not parts.hostname:
    raise SystemExit("CONAN_HTTPS_PROXY_URL must include scheme and host")
host = parts.hostname
if ":" in host and not host.startswith("["):
    host = f"[{host}]"
port = f":{parts.port}" if parts.port else ""
netloc = f"{username}:{password}@{host}{port}"
print(urlunsplit((parts.scheme, netloc, parts.path, parts.query, parts.fragment)))
PY
)"

if [ -n "${GITHUB_ACTIONS:-}" ]; then
  echo "::add-mask::$proxy_with_auth"
fi
export HTTPS_PROXY="$proxy_with_auth"
export https_proxy="$proxy_with_auth"

conan remote add "$remote_name" "$remote_url" -f
conan remote login "$remote_name" "$username" -p "$password"
conan upload "$reference:*" -r "$remote_name" --check -c
