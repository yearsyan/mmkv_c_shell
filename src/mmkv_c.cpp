#include "mmkv_c/mmkv_c.h"

#include <MMKV/MMKV.h>

#include <algorithm>
#include <cstring>
#include <string>

#if defined(MMKV_APPLE)
using MMKVNative = mmkv::MMKV;
using MMKVConfigNative = mmkv::MMKVConfig;
using MMKVModeNative = mmkv::MMKVMode;
using MMKVLogLevelNative = mmkv::MMKVLogLevel;
using MMKVRecoverStrategicNative = mmkv::MMKVRecoverStrategic;
using SyncFlagNative = mmkv::SyncFlag;
#else
using MMKVNative = MMKV;
using MMKVConfigNative = MMKVConfig;
using MMKVModeNative = MMKVMode;
using MMKVLogLevelNative = MMKVLogLevel;
using MMKVRecoverStrategicNative = MMKVRecoverStrategic;
using SyncFlagNative = SyncFlag;
#endif

namespace {

MMKVNative *to_kv(mmkv_c_handle *handle) {
    return reinterpret_cast<MMKVNative *>(handle);
}

mmkv_c_handle *to_handle(MMKVNative *kv) {
    return reinterpret_cast<mmkv_c_handle *>(kv);
}

bool valid_key(const char *key) {
    return key != nullptr && key[0] != '\0';
}

MMKVPath_t to_path(const char *path) {
    return path ? string2MMKVPath_t(path) : MMKVPath_t();
}

MMKVLogLevelNative to_log_level(mmkv_c_log_level level) {
    switch (level) {
        case MMKV_C_LOG_DEBUG:
            return MMKVLogDebug;
        case MMKV_C_LOG_INFO:
            return MMKVLogInfo;
        case MMKV_C_LOG_WARNING:
            return MMKVLogWarning;
        case MMKV_C_LOG_ERROR:
            return MMKVLogError;
        case MMKV_C_LOG_NONE:
            return MMKVLogNone;
    }
    return MMKVLogInfo;
}

MMKVConfigNative to_native_config(const mmkv_c_config *config, std::string &crypt_key, MMKVPath_t &root_dir) {
    MMKVConfigNative native;
    native.mode = MMKV_SINGLE_PROCESS;

    if (config == nullptr) {
        return native;
    }

    native.mode = static_cast<MMKVModeNative>(config->mode == 0 ? MMKV_C_SINGLE_PROCESS : config->mode);
    native.expectedCapacity = config->expected_capacity;
    native.aes256 = config->aes256;
    native.expiredInSeconds = config->expired_in_seconds;
    native.enableCompareBeforeSet = config->enable_compare_before_set;
    native.itemSizeLimit = config->item_size_limit;

    if (config->crypt_key != nullptr && config->crypt_key[0] != '\0') {
        crypt_key = config->crypt_key;
        native.cryptKey = &crypt_key;
    }

    if (config->root_dir != nullptr && config->root_dir[0] != '\0') {
        root_dir = to_path(config->root_dir);
        native.rootPath = &root_dir;
    }

    if (config->enable_key_expire >= 0) {
        native.enableKeyExpire = config->enable_key_expire != 0;
    }

    if (config->recover_strategy >= 0) {
        native.recover = static_cast<MMKVRecoverStrategicNative>(config->recover_strategy);
    }

    return native;
}

mmkv_c_result copy_string_result(const std::string &value, char *buffer, size_t *inout_size) {
    if (inout_size == nullptr) {
        return MMKV_C_INVALID_ARGUMENT;
    }

    const size_t required = value.size() + 1;
    const size_t capacity = *inout_size;
    *inout_size = required;

    if (buffer == nullptr || capacity < required) {
        return MMKV_C_BUFFER_TOO_SMALL;
    }

    if (!value.empty()) {
        std::memcpy(buffer, value.data(), value.size());
    }
    buffer[value.size()] = '\0';
    return MMKV_C_OK;
}

mmkv_c_result copy_bytes_result(const mmkv::MMBuffer &value, void *buffer, size_t *inout_size) {
    if (inout_size == nullptr) {
        return MMKV_C_INVALID_ARGUMENT;
    }

    const size_t required = value.length();
    const size_t capacity = *inout_size;
    *inout_size = required;

    if (required == 0) {
        return MMKV_C_OK;
    }

    if (buffer == nullptr || capacity < required) {
        return MMKV_C_BUFFER_TOO_SMALL;
    }

    std::memcpy(buffer, value.getPtr(), required);
    return MMKV_C_OK;
}

} // namespace

const char *mmkv_c_version(void) {
    return MMKV_VERSION;
}

mmkv_c_config mmkv_c_default_config(void) {
    mmkv_c_config config = {};
    config.mode = MMKV_C_SINGLE_PROCESS;
    config.recover_strategy = MMKV_C_RECOVER_DEFAULT;
    config.enable_key_expire = -1;
    return config;
}

mmkv_c_result mmkv_c_initialize(const char *root_dir, mmkv_c_log_level log_level) {
    if (root_dir == nullptr || root_dir[0] == '\0') {
        return MMKV_C_INVALID_ARGUMENT;
    }

    try {
        auto root = to_path(root_dir);
        MMKVNative::initializeMMKV(root, to_log_level(log_level));
        return MMKV_C_OK;
    } catch (...) {
        return MMKV_C_ERROR;
    }
}

void mmkv_c_on_exit(void) {
    try {
        MMKVNative::onExit();
    } catch (...) {
    }
}

mmkv_c_handle *mmkv_c_open(const char *mmap_id, const mmkv_c_config *config) {
    if (mmap_id == nullptr || mmap_id[0] == '\0') {
        return nullptr;
    }

    try {
        std::string crypt_key;
        MMKVPath_t root_dir;
        auto native = to_native_config(config, crypt_key, root_dir);
        return to_handle(MMKVNative::mmkvWithID(std::string(mmap_id), native));
    } catch (...) {
        return nullptr;
    }
}

mmkv_c_handle *mmkv_c_default(const mmkv_c_config *config) {
    try {
        std::string crypt_key;
        MMKVPath_t root_dir;
        auto native = to_native_config(config, crypt_key, root_dir);
        return to_handle(MMKVNative::defaultMMKV(native));
    } catch (...) {
        return nullptr;
    }
}

void mmkv_c_close(mmkv_c_handle *handle) {
    try {
        if (auto *kv = to_kv(handle)) {
            kv->close();
        }
    } catch (...) {
    }
}

const char *mmkv_c_mmap_id(mmkv_c_handle *handle) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->mmapID().c_str();
        }
    } catch (...) {
    }
    return nullptr;
}

bool mmkv_c_is_multi_process(mmkv_c_handle *handle) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->isMultiProcess();
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_is_read_only(mmkv_c_handle *handle) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->isReadOnly();
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_set_bool(mmkv_c_handle *handle, const char *key, bool value) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->set(value, std::string(key));
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_set_int32(mmkv_c_handle *handle, const char *key, int32_t value) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->set(value, std::string(key));
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_set_uint32(mmkv_c_handle *handle, const char *key, uint32_t value) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->set(value, std::string(key));
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_set_int64(mmkv_c_handle *handle, const char *key, int64_t value) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->set(value, std::string(key));
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_set_uint64(mmkv_c_handle *handle, const char *key, uint64_t value) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->set(value, std::string(key));
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_set_float(mmkv_c_handle *handle, const char *key, float value) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->set(value, std::string(key));
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_set_double(mmkv_c_handle *handle, const char *key, double value) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->set(value, std::string(key));
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_set_string(mmkv_c_handle *handle, const char *key, const char *value) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key) && value != nullptr) {
            return kv->set(std::string_view(value), std::string(key));
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_set_bytes(mmkv_c_handle *handle, const char *key, const void *value, size_t value_size) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key) && (value != nullptr || value_size == 0)) {
            auto bytes = value_size == 0
                ? mmkv::MMBuffer(0)
                : mmkv::MMBuffer(const_cast<void *>(value), value_size, mmkv::MMBufferNoCopy);
            return kv->set(bytes, std::string(key));
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_get_bool(mmkv_c_handle *handle, const char *key, bool default_value, bool *has_value) {
    bool local_has_value = false;
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->getBool(std::string(key), default_value, has_value ? has_value : &local_has_value);
        }
    } catch (...) {
    }
    if (has_value) {
        *has_value = false;
    }
    return default_value;
}

int32_t mmkv_c_get_int32(mmkv_c_handle *handle, const char *key, int32_t default_value, bool *has_value) {
    bool local_has_value = false;
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->getInt32(std::string(key), default_value, has_value ? has_value : &local_has_value);
        }
    } catch (...) {
    }
    if (has_value) {
        *has_value = false;
    }
    return default_value;
}

uint32_t mmkv_c_get_uint32(mmkv_c_handle *handle, const char *key, uint32_t default_value, bool *has_value) {
    bool local_has_value = false;
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->getUInt32(std::string(key), default_value, has_value ? has_value : &local_has_value);
        }
    } catch (...) {
    }
    if (has_value) {
        *has_value = false;
    }
    return default_value;
}

int64_t mmkv_c_get_int64(mmkv_c_handle *handle, const char *key, int64_t default_value, bool *has_value) {
    bool local_has_value = false;
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->getInt64(std::string(key), default_value, has_value ? has_value : &local_has_value);
        }
    } catch (...) {
    }
    if (has_value) {
        *has_value = false;
    }
    return default_value;
}

uint64_t mmkv_c_get_uint64(mmkv_c_handle *handle, const char *key, uint64_t default_value, bool *has_value) {
    bool local_has_value = false;
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->getUInt64(std::string(key), default_value, has_value ? has_value : &local_has_value);
        }
    } catch (...) {
    }
    if (has_value) {
        *has_value = false;
    }
    return default_value;
}

float mmkv_c_get_float(mmkv_c_handle *handle, const char *key, float default_value, bool *has_value) {
    bool local_has_value = false;
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->getFloat(std::string(key), default_value, has_value ? has_value : &local_has_value);
        }
    } catch (...) {
    }
    if (has_value) {
        *has_value = false;
    }
    return default_value;
}

double mmkv_c_get_double(mmkv_c_handle *handle, const char *key, double default_value, bool *has_value) {
    bool local_has_value = false;
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->getDouble(std::string(key), default_value, has_value ? has_value : &local_has_value);
        }
    } catch (...) {
    }
    if (has_value) {
        *has_value = false;
    }
    return default_value;
}

mmkv_c_result mmkv_c_get_string(mmkv_c_handle *handle, const char *key, char *buffer, size_t *inout_size) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            std::string value;
            if (!kv->getString(std::string(key), value)) {
                return MMKV_C_NOT_FOUND;
            }
            return copy_string_result(value, buffer, inout_size);
        }
    } catch (...) {
        return MMKV_C_ERROR;
    }
    return MMKV_C_INVALID_ARGUMENT;
}

mmkv_c_result mmkv_c_get_bytes(mmkv_c_handle *handle, const char *key, void *buffer, size_t *inout_size) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            mmkv::MMBuffer value;
            if (!kv->getBytes(std::string(key), value)) {
                return MMKV_C_NOT_FOUND;
            }
            return copy_bytes_result(value, buffer, inout_size);
        }
    } catch (...) {
        return MMKV_C_ERROR;
    }
    return MMKV_C_INVALID_ARGUMENT;
}

bool mmkv_c_contains_key(mmkv_c_handle *handle, const char *key) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->containsKey(std::string(key));
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_remove_value(mmkv_c_handle *handle, const char *key) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->removeValueForKey(std::string(key));
        }
    } catch (...) {
    }
    return false;
}

void mmkv_c_clear_all(mmkv_c_handle *handle, bool keep_space) {
    try {
        if (auto *kv = to_kv(handle)) {
            kv->clearAll(keep_space);
        }
    } catch (...) {
    }
}

void mmkv_c_trim(mmkv_c_handle *handle) {
    try {
        if (auto *kv = to_kv(handle)) {
            kv->trim();
        }
    } catch (...) {
    }
}

void mmkv_c_sync(mmkv_c_handle *handle, bool blocking) {
    try {
        if (auto *kv = to_kv(handle)) {
            kv->sync(blocking ? MMKV_SYNC : MMKV_ASYNC);
        }
    } catch (...) {
    }
}

void mmkv_c_clear_memory_cache(mmkv_c_handle *handle, bool keep_space) {
    try {
        if (auto *kv = to_kv(handle)) {
            kv->clearMemoryCache(keep_space);
        }
    } catch (...) {
    }
}

size_t mmkv_c_count(mmkv_c_handle *handle, bool filter_expired) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->count(filter_expired);
        }
    } catch (...) {
    }
    return 0;
}

size_t mmkv_c_total_size(mmkv_c_handle *handle) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->totalSize();
        }
    } catch (...) {
    }
    return 0;
}

size_t mmkv_c_actual_size(mmkv_c_handle *handle) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->actualSize();
        }
    } catch (...) {
    }
    return 0;
}

size_t mmkv_c_value_size(mmkv_c_handle *handle, const char *key, bool actual_size) {
    try {
        if (auto *kv = to_kv(handle); kv && valid_key(key)) {
            return kv->getValueSize(std::string(key), actual_size);
        }
    } catch (...) {
    }
    return 0;
}

bool mmkv_c_rekey(mmkv_c_handle *handle, const char *crypt_key, bool aes256) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->reKey(crypt_key ? std::string(crypt_key) : std::string(), aes256);
        }
    } catch (...) {
    }
    return false;
}

void mmkv_c_check_reset_crypt_key(mmkv_c_handle *handle, const char *crypt_key, bool aes256) {
    try {
        if (auto *kv = to_kv(handle)) {
            if (crypt_key != nullptr && crypt_key[0] != '\0') {
                std::string key = crypt_key;
                kv->checkReSetCryptKey(&key, aes256);
            } else {
                kv->checkReSetCryptKey(nullptr, aes256);
            }
        }
    } catch (...) {
    }
}

bool mmkv_c_enable_auto_key_expire(mmkv_c_handle *handle, uint32_t expired_in_seconds) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->enableAutoKeyExpire(expired_in_seconds);
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_disable_auto_key_expire(mmkv_c_handle *handle) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->disableAutoKeyExpire();
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_enable_compare_before_set(mmkv_c_handle *handle) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->enableCompareBeforeSet();
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_disable_compare_before_set(mmkv_c_handle *handle) {
    try {
        if (auto *kv = to_kv(handle)) {
            return kv->disableCompareBeforeSet();
        }
    } catch (...) {
    }
    return false;
}

bool mmkv_c_backup_one(const char *mmap_id, const char *dst_dir, const char *src_dir) {
    if (!valid_key(mmap_id) || dst_dir == nullptr || dst_dir[0] == '\0') {
        return false;
    }

    try {
        auto dst = to_path(dst_dir);
        auto src = to_path(src_dir);
        return MMKVNative::backupOneToDirectory(std::string(mmap_id), dst, src_dir ? &src : nullptr);
    } catch (...) {
        return false;
    }
}

bool mmkv_c_restore_one(const char *mmap_id, const char *src_dir, const char *dst_dir) {
    if (!valid_key(mmap_id) || src_dir == nullptr || src_dir[0] == '\0') {
        return false;
    }

    try {
        auto src = to_path(src_dir);
        auto dst = to_path(dst_dir);
        return MMKVNative::restoreOneFromDirectory(std::string(mmap_id), src, dst_dir ? &dst : nullptr);
    } catch (...) {
        return false;
    }
}

size_t mmkv_c_backup_all(const char *dst_dir, const char *src_dir) {
    if (dst_dir == nullptr || dst_dir[0] == '\0') {
        return 0;
    }

    try {
        auto dst = to_path(dst_dir);
        auto src = to_path(src_dir);
        return MMKVNative::backupAllToDirectory(dst, src_dir ? &src : nullptr);
    } catch (...) {
        return 0;
    }
}

size_t mmkv_c_restore_all(const char *src_dir, const char *dst_dir) {
    if (src_dir == nullptr || src_dir[0] == '\0') {
        return 0;
    }

    try {
        auto src = to_path(src_dir);
        auto dst = to_path(dst_dir);
        return MMKVNative::restoreAllFromDirectory(src, dst_dir ? &dst : nullptr);
    } catch (...) {
        return 0;
    }
}

bool mmkv_c_is_file_valid(const char *mmap_id, const char *root_dir) {
    if (!valid_key(mmap_id)) {
        return false;
    }

    try {
        auto root = to_path(root_dir);
        return MMKVNative::isFileValid(std::string(mmap_id), root_dir ? &root : nullptr);
    } catch (...) {
        return false;
    }
}

bool mmkv_c_remove_storage(const char *mmap_id, const char *root_dir) {
    if (!valid_key(mmap_id)) {
        return false;
    }

    try {
        auto root = to_path(root_dir);
        return MMKVNative::removeStorage(std::string(mmap_id), root_dir ? &root : nullptr);
    } catch (...) {
        return false;
    }
}

bool mmkv_c_check_exist(const char *mmap_id, const char *root_dir) {
    if (!valid_key(mmap_id)) {
        return false;
    }

    try {
        auto root = to_path(root_dir);
        return MMKVNative::checkExist(std::string(mmap_id), root_dir ? &root : nullptr);
    } catch (...) {
        return false;
    }
}
