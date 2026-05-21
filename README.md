# mmkv_c

This repository packages Tencent MMKV as static libraries with a small C API.
It is intended for engines such as Godot that want to link MMKV directly into
the engine binary and expose their own engine-facing classes.

## Build

```bash
cmake -S . -B build -DMMKV_C_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The build produces static archives only:

- `libmmkv_c.a`: the C facade.
- `libcore.a`: MMKV core.

On Windows the archive names use the platform's normal static library suffix.
`MMKV_C_FORCE_POSIX` defaults to `ON`, so Apple and Android builds avoid the
Objective-C/JNI wrapper layers and use MMKV's POSIX core path for engine
embedding.

## Conan 2

```bash
conan create . --build=missing
```

The recipe has a standard `test_package`, so `conan create` also verifies that
a downstream CMake consumer can use `find_package(mmkv_c CONFIG REQUIRED)` and
link `mmkv_c::mmkv_c`.

Consumer side:

```cmake
find_package(mmkv_c CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE mmkv_c::mmkv_c)
```

The package exposes `include/mmkv_c/mmkv_c.h`. Consumers do not need MMKV's C++
headers.

## Temporary LAN Conan Remote

This repository includes a tiny Conan 2 test remote for LAN-only package
publishing. It is file-backed and intended for temporary CI tests, not for
production package hosting.

Start the server on the build host:

```bash
python3 tools/conan_test_server.py \
  --host 0.0.0.0 \
  --port 9300 \
  --storage .conan-test-server/storage
```

Publish this package from the checkout:

```bash
CONAN_REMOTE_URL=http://127.0.0.1:9300 tools/publish_to_temp_conan.sh
```

Manual equivalent:

```bash
conan remote add temp-conan http://127.0.0.1:9300 -f
conan create . --build=missing -s build_type=Release -nr
conan upload "mmkv_c/2.4.1:*" -r temp-conan --check -c
```

Optional auth:

```bash
CONAN_TEST_SERVER_USER=ci \
CONAN_TEST_SERVER_PASSWORD=<password> \
python3 tools/conan_test_server.py --host 0.0.0.0 --port 9300
```

With no password configured on the server, uploads are anonymous inside the LAN.
This temporary server is only for local/LAN validation; CI publishing uses the
private remote configured through repository secrets.

The server stores all data below `.conan-test-server/storage`; remove that
directory to reset the temporary remote. S3 credentials are available in the
Godot pannel local config if a future storage backend needs them, but this
test server deliberately does not read or commit those credentials.

Private Conan remote example:

```bash
conan remote add private https://conan.example.internal -f
conan remote login private <user> -p <password-or-token>
conan upload "mmkv_c/2.4.1:*" -r private --check -c
```

CI builds run on regular validation events without uploading packages. Publishing
runs only for `v**` tag pushes and uploads through an HTTPS proxy. Package
uploads require these repository secrets:

- `CONAN_REMOTE_URL`: private Conan remote URL, for example your internal
  `https://...` endpoint.
- `CONAN_USERNAME`: Conan username or service account.
- `CONAN_PASSWORD`: Conan password or personal access token.
- `CONAN_HTTPS_PROXY_URL`: HTTPS proxy URL, including scheme and host.
- `CONAN_HTTPS_PROXY_USERNAME`: HTTPS proxy username.
- `CONAN_HTTPS_PROXY_PASSWORD`: HTTPS proxy password.

The private remote URL and proxy credentials are intentionally not hard-coded in
either workflow.

For the Godot/SCons engine module, the current integration pattern is:

```bash
conan remote add private <CONAN_REMOTE_URL> -f
conan remote login private <user> -p <password-or-token>
GODOT_MMKV_CONAN_REMOTE=private scons platform=<platform> target=<target> arch=<arch>
```

For local development against this checkout instead of the remote package:

```bash
GODOT_MMKV_CONAN_EXPORT_PATH=/Users/u/workspace/mmkv-lib \
GODOT_MMKV_CONAN_NO_REMOTE=1 \
scons platform=<platform> target=<target> arch=<arch>
```

This package is intentionally independent from Godot; Godot headers and
`GDCLASS` wrappers live in the engine project.

## C API Shape

The facade uses opaque handles and caller-owned buffers:

```c
#include <mmkv_c/mmkv_c.h>

mmkv_c_initialize("/path/to/mmkv", MMKV_C_LOG_INFO);

mmkv_c_config config = mmkv_c_default_config();
mmkv_c_handle *kv = mmkv_c_open("game.settings", &config);

mmkv_c_set_string(kv, "language", "zh");

size_t size = 0;
mmkv_c_get_string(kv, "language", NULL, &size);
char *buffer = ...;
mmkv_c_get_string(kv, "language", buffer, &size);

mmkv_c_close(kv);
mmkv_c_on_exit();
```

For Godot, the engine module can wrap this C layer in a `GDCLASS` and keep all
Godot headers out of this package.
