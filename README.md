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

Internal remote example:

```bash
conan remote add company-conan https://conan.example.internal/artifactory/api/conan/conan-local
conan remote login company-conan <user>
conan upload "mmkv_c/2.4.0:*" -r=company-conan --confirm
```

For Godot/SCons consumers, run `conan install` before the engine build and feed
the resolved package include/lib paths into the module `SCsub`. This package is
already intentionally independent from Godot.

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
