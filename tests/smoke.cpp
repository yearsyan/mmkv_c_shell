#include "mmkv_c/mmkv_c.h"

#include <array>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <string>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "mmkv_c_smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    assert(mmkv_c_initialize(root.string().c_str(), MMKV_C_LOG_NONE) == MMKV_C_OK);

    auto config = mmkv_c_default_config();
    auto *kv = mmkv_c_open("godot.smoke", &config);
    assert(kv != nullptr);

    assert(mmkv_c_set_bool(kv, "ready", true));
    assert(mmkv_c_set_int64(kv, "score", 42));
    assert(mmkv_c_set_double(kv, "ratio", 1.5));
    assert(mmkv_c_set_string(kv, "name", "GachaGameProducer"));

    const std::array<unsigned char, 4> payload = {1, 2, 3, 4};
    assert(mmkv_c_set_bytes(kv, "payload", payload.data(), payload.size()));

    bool has_value = false;
    assert(mmkv_c_get_bool(kv, "ready", false, &has_value));
    assert(has_value);
    assert(mmkv_c_get_int64(kv, "score", 0, &has_value) == 42);
    assert(has_value);
    assert(mmkv_c_get_double(kv, "ratio", 0.0, &has_value) == 1.5);
    assert(has_value);

    size_t string_size = 0;
    assert(mmkv_c_get_string(kv, "name", nullptr, &string_size) == MMKV_C_BUFFER_TOO_SMALL);
    std::string name(string_size, '\0');
    assert(mmkv_c_get_string(kv, "name", name.data(), &string_size) == MMKV_C_OK);
    assert(std::strcmp(name.c_str(), "GachaGameProducer") == 0);

    size_t bytes_size = 0;
    assert(mmkv_c_get_bytes(kv, "payload", nullptr, &bytes_size) == MMKV_C_BUFFER_TOO_SMALL);
    std::array<unsigned char, 4> out = {};
    assert(bytes_size == out.size());
    assert(mmkv_c_get_bytes(kv, "payload", out.data(), &bytes_size) == MMKV_C_OK);
    assert(out == payload);

    assert(mmkv_c_contains_key(kv, "score"));
    assert(mmkv_c_remove_value(kv, "score"));
    assert(!mmkv_c_contains_key(kv, "score"));

    mmkv_c_sync(kv, true);
    mmkv_c_close(kv);
    mmkv_c_on_exit();

    std::filesystem::remove_all(root);
    return 0;
}
