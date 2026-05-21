#include "mmkv_c/mmkv_c.h"

int main(void) {
    mmkv_c_config config = mmkv_c_default_config();
    return config.mode == MMKV_C_SINGLE_PROCESS ? 0 : 1;
}
