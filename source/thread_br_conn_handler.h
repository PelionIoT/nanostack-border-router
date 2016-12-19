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
/* Thread network status */
void thread_br_conn_handler_update_thread_connection(bool status);

/* Ethernet connection status */
void thread_br_conn_handler_update_ethernet_connection(bool status);

void thread_br_conn_handler_eth_ready();
// Setters
void   thread_br_conn_handler_threadinterface_id_set(int8_t interfaceId);
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

