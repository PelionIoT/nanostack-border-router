/*
 * Copyright (c) 2016-2017, Pelion and affiliates.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t rf_device_register(void);
void rf_read_mac_address(uint8_t *mac);
#ifdef __cplusplus
}
#endif
