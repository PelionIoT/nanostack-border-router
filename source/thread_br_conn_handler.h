/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#ifndef thread_br_conn_handler_H_
#define thread_br_conn_handler_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ns_types.h"

void thread_br_conn_handler_init(void);
void thread_br_conn_handler_thread_connection_update(bool status);
void thread_br_conn_handler_ethernet_connection_update(bool status);

// Tells that ethernet connection is ready and the prefix can be read and set.
void thread_br_conn_handler_eth_ready();
// Setters
void   thread_br_conn_handler_thread_interface_id_set(int8_t interfaceId);
void   thread_br_conn_handler_eth_interface_id_set(int8_t interfaceId);

// Getters thread_br_conn_handler       
bool   thread_br_conn_handler_eth_connection_status_get(void);
bool   thread_br_conn_handler_thread_connection_status_get(void);
int8_t thread_br_conn_handler_thread_interface_id_get(void);
int8_t thread_br_conn_handler_eth_interface_id_get(void);


#ifdef __cplusplus
}
#endif
#endif /* thread_br_conn_handler_H_ */

