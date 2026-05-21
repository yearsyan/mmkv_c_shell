#include <mmkv_c/mmkv_c.h>

#include <cassert>
#include <filesystem>
#include <string>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "mmkv_c_conan_test_package";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    assert(mmkv_c_initialize(root.string().c_str(), MMKV_C_LOG_NONE) == MMKV_C_OK);

    mmkv_c_config config = mmkv_c_default_config();
    mmkv_c_handle *kv = mmkv_c_open("consumer", &config);
    assert(kv != nullptr);

    assert(mmkv_c_set_int32(kv, "answer", 42));

    bool has_value = false;
    assert(mmkv_c_get_int32(kv, "answer", 0, &has_value) == 42);
    assert(has_value);

    mmkv_c_close(kv);
    mmkv_c_on_exit();
    std::filesystem::remove_all(root);
    return 0;
}
