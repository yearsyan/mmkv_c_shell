#ifndef MMKV_C_MMKV_C_H
#define MMKV_C_MMKV_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(MMKV_C_SHARED)
#  if defined(MMKV_C_BUILDING)
#    define MMKV_C_API __declspec(dllexport)
#  else
#    define MMKV_C_API __declspec(dllimport)
#  endif
#else
#  define MMKV_C_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mmkv_c_handle mmkv_c_handle;

typedef enum mmkv_c_result {
    MMKV_C_OK = 0,
    MMKV_C_NOT_FOUND = 1,
    MMKV_C_BUFFER_TOO_SMALL = 2,
    MMKV_C_INVALID_ARGUMENT = -1,
    MMKV_C_ERROR = -2
} mmkv_c_result;

typedef enum mmkv_c_log_level {
    MMKV_C_LOG_DEBUG = 0,
    MMKV_C_LOG_INFO = 1,
    MMKV_C_LOG_WARNING = 2,
    MMKV_C_LOG_ERROR = 3,
    MMKV_C_LOG_NONE = 4
} mmkv_c_log_level;

typedef enum mmkv_c_recover_strategy {
    MMKV_C_RECOVER_DISCARD = 0,
    MMKV_C_RECOVER_RECOVER = 1,
    MMKV_C_RECOVER_DEFAULT = -1
} mmkv_c_recover_strategy;

enum {
    MMKV_C_SINGLE_PROCESS = 1u << 0,
    MMKV_C_MULTI_PROCESS = 1u << 1,
    MMKV_C_READ_ONLY = 1u << 5
};

typedef struct mmkv_c_config {
    uint32_t mode;
    const char *root_dir;
    const char *crypt_key;
    size_t expected_capacity;
    bool aes256;
    int32_t enable_key_expire;
    uint32_t expired_in_seconds;
    bool enable_compare_before_set;
    int32_t recover_strategy;
    uint32_t item_size_limit;
} mmkv_c_config;

MMKV_C_API const char *mmkv_c_version(void);
MMKV_C_API mmkv_c_config mmkv_c_default_config(void);

MMKV_C_API mmkv_c_result mmkv_c_initialize(const char *root_dir, mmkv_c_log_level log_level);
MMKV_C_API void mmkv_c_on_exit(void);

MMKV_C_API mmkv_c_handle *mmkv_c_open(const char *mmap_id, const mmkv_c_config *config);
MMKV_C_API mmkv_c_handle *mmkv_c_default(const mmkv_c_config *config);
MMKV_C_API void mmkv_c_close(mmkv_c_handle *handle);

MMKV_C_API const char *mmkv_c_mmap_id(mmkv_c_handle *handle);
MMKV_C_API bool mmkv_c_is_multi_process(mmkv_c_handle *handle);
MMKV_C_API bool mmkv_c_is_read_only(mmkv_c_handle *handle);

MMKV_C_API bool mmkv_c_set_bool(mmkv_c_handle *handle, const char *key, bool value);
MMKV_C_API bool mmkv_c_set_int32(mmkv_c_handle *handle, const char *key, int32_t value);
MMKV_C_API bool mmkv_c_set_uint32(mmkv_c_handle *handle, const char *key, uint32_t value);
MMKV_C_API bool mmkv_c_set_int64(mmkv_c_handle *handle, const char *key, int64_t value);
MMKV_C_API bool mmkv_c_set_uint64(mmkv_c_handle *handle, const char *key, uint64_t value);
MMKV_C_API bool mmkv_c_set_float(mmkv_c_handle *handle, const char *key, float value);
MMKV_C_API bool mmkv_c_set_double(mmkv_c_handle *handle, const char *key, double value);
MMKV_C_API bool mmkv_c_set_string(mmkv_c_handle *handle, const char *key, const char *value);
MMKV_C_API bool mmkv_c_set_bytes(mmkv_c_handle *handle, const char *key, const void *value, size_t value_size);

MMKV_C_API bool mmkv_c_get_bool(mmkv_c_handle *handle, const char *key, bool default_value, bool *has_value);
MMKV_C_API int32_t mmkv_c_get_int32(mmkv_c_handle *handle, const char *key, int32_t default_value, bool *has_value);
MMKV_C_API uint32_t mmkv_c_get_uint32(mmkv_c_handle *handle, const char *key, uint32_t default_value, bool *has_value);
MMKV_C_API int64_t mmkv_c_get_int64(mmkv_c_handle *handle, const char *key, int64_t default_value, bool *has_value);
MMKV_C_API uint64_t mmkv_c_get_uint64(mmkv_c_handle *handle, const char *key, uint64_t default_value, bool *has_value);
MMKV_C_API float mmkv_c_get_float(mmkv_c_handle *handle, const char *key, float default_value, bool *has_value);
MMKV_C_API double mmkv_c_get_double(mmkv_c_handle *handle, const char *key, double default_value, bool *has_value);

/*
 * String buffers include the trailing '\0' in inout_size.
 * Pass buffer = NULL or *inout_size = 0 to query the required size.
 */
MMKV_C_API mmkv_c_result mmkv_c_get_string(mmkv_c_handle *handle, const char *key, char *buffer, size_t *inout_size);

/*
 * Byte buffers do not include a terminator in inout_size.
 * Pass buffer = NULL or *inout_size = 0 to query the required size.
 */
MMKV_C_API mmkv_c_result mmkv_c_get_bytes(mmkv_c_handle *handle, const char *key, void *buffer, size_t *inout_size);

MMKV_C_API bool mmkv_c_contains_key(mmkv_c_handle *handle, const char *key);
MMKV_C_API bool mmkv_c_remove_value(mmkv_c_handle *handle, const char *key);
MMKV_C_API void mmkv_c_clear_all(mmkv_c_handle *handle, bool keep_space);
MMKV_C_API void mmkv_c_trim(mmkv_c_handle *handle);
MMKV_C_API void mmkv_c_sync(mmkv_c_handle *handle, bool blocking);
MMKV_C_API void mmkv_c_clear_memory_cache(mmkv_c_handle *handle, bool keep_space);

MMKV_C_API size_t mmkv_c_count(mmkv_c_handle *handle, bool filter_expired);
MMKV_C_API size_t mmkv_c_total_size(mmkv_c_handle *handle);
MMKV_C_API size_t mmkv_c_actual_size(mmkv_c_handle *handle);
MMKV_C_API size_t mmkv_c_value_size(mmkv_c_handle *handle, const char *key, bool actual_size);

MMKV_C_API bool mmkv_c_rekey(mmkv_c_handle *handle, const char *crypt_key, bool aes256);
MMKV_C_API void mmkv_c_check_reset_crypt_key(mmkv_c_handle *handle, const char *crypt_key, bool aes256);

MMKV_C_API bool mmkv_c_enable_auto_key_expire(mmkv_c_handle *handle, uint32_t expired_in_seconds);
MMKV_C_API bool mmkv_c_disable_auto_key_expire(mmkv_c_handle *handle);
MMKV_C_API bool mmkv_c_enable_compare_before_set(mmkv_c_handle *handle);
MMKV_C_API bool mmkv_c_disable_compare_before_set(mmkv_c_handle *handle);

MMKV_C_API bool mmkv_c_backup_one(const char *mmap_id, const char *dst_dir, const char *src_dir);
MMKV_C_API bool mmkv_c_restore_one(const char *mmap_id, const char *src_dir, const char *dst_dir);
MMKV_C_API size_t mmkv_c_backup_all(const char *dst_dir, const char *src_dir);
MMKV_C_API size_t mmkv_c_restore_all(const char *src_dir, const char *dst_dir);
MMKV_C_API bool mmkv_c_is_file_valid(const char *mmap_id, const char *root_dir);
MMKV_C_API bool mmkv_c_remove_storage(const char *mmap_id, const char *root_dir);
MMKV_C_API bool mmkv_c_check_exist(const char *mmap_id, const char *root_dir);

#ifdef __cplusplus
}
#endif

#endif
