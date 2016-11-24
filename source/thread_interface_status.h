/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#ifndef THREAD_INTERFACE_STATUS_H_
#define THREAD_INTERFACE_STATUS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ns_types.h"

void thread_interface_status_init(void);
/* Thread network status */
void thread_interface_status_thread_connection(bool status);

/* Ethernet connection status */
void thread_interface_status_ethernet_connection(bool status);

void thread_interface_status_prefix_add(uint8_t *prefix, uint8_t prefix_len);
// Setters
void   thread_interface_status_thread_driver_id_set(int8_t driverId);
void   thread_interface_status_threadinterface_id_set(int8_t interfaceId);
void   thread_interface_status_borderrouter_driver_id_set(int8_t driverId);
void   thread_interface_status_borderrouter_interface_id_set(int8_t interfaceId);

// Getters
bool   thread_interface_status_borderconnected_status_get(void);
bool   thread_interface_status_threadconnected_status_get(void);
int8_t thread_interface_status_thread_interface_id_get(void);
int8_t thread_interface_status_thread_driver_id_get(void);
int8_t thread_interface_status_border_router_interface_id_get(void);
int8_t thread_interface_status_border_router_driver_id_get(void);


#ifdef __cplusplus
}
#endif
#endif /* THREAD_INTERFACE_STATUS_H_ */

