/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#ifndef WISUN_BR_CONN_HANDLER_H_
#define WISUN_BR_CONN_HANDLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ns_types.h"

void wisun_br_conn_handler_init(void);
void wisun_br_conn_handler_wisun_connection_update(bool status);
void wisun_br_conn_handler_ethernet_connection_update(bool status);

// Tells that ethernet connection is ready and the prefix can be read and set.
void wisun_br_conn_handler_eth_ready(void);
// Setters
void   wisun_br_conn_handler_wisun_interface_id_set(int8_t interfaceId);
void   wisun_br_conn_handler_eth_interface_id_set(int8_t interfaceId);

// Getters thread_br_conn_handler       
bool   wisun_br_conn_handler_eth_connection_status_get(void);
bool   wisun_br_conn_handler_wisun_connection_status_get(void);
int8_t wisun_br_conn_handler_wisun_interface_id_get(void);
int8_t wisun_br_conn_handler_eth_interface_id_get(void);


#ifdef __cplusplus
}
#endif
#endif /* WISUN_BR_CONN_HANDLER_H_ */
