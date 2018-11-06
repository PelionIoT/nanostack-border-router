/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#include <string.h>
#include "net_interface.h"
#include "mbed-trace/mbed_trace.h"
#include "ws_bbr_api.h"
#include "wisun_br_conn_handler.h"
#include "borderrouter_helpers.h"
#include "common_functions.h"
#include "eventOS_event_timer.h"
#include "multicast_api.h"

#define TRACE_GROUP "WBRH"

typedef struct {
    
    int8_t  ws_interface_id;
    int8_t  eth_interface_id;
    bool    eth_connection_ready;
    bool    ws_connection_ready;
} ws_br_handler_t;

static ws_br_handler_t ws_br_handler;

void wisun_br_conn_handler_init(void)
{
	ws_br_handler.ws_interface_id = -1;
    ws_br_handler.ws_connection_ready = 0;

	ws_br_handler.eth_interface_id = -1;
    ws_br_handler.eth_connection_ready = 0;
}

void wisun_br_conn_handler_wisun_connection_update(bool status)
{
    ws_br_handler.ws_connection_ready = status;
    tr_debug("mesh0 connection status: %d", status);

}

void wisun_br_conn_handler_ethernet_connection_update(bool status)
{
    ws_br_handler.eth_connection_ready = status;
    tr_debug("Eth0 connection status: %d", status);

}

void wisun_br_conn_handler_wisun_interface_id_set(int8_t interfaceId)
{
    ws_br_handler.ws_interface_id = interfaceId;
    if (ws_br_handler.ws_interface_id > -1 &&
  		ws_br_handler.eth_interface_id > -1) {
    	ws_bbr_start(ws_br_handler.ws_interface_id, ws_br_handler.eth_interface_id);
    }
}

int8_t wisun_br_conn_handler_wisun_interface_id_get(void)
{
    return ws_br_handler.ws_interface_id;
}

bool wisun_br_conn_handler_eth_connection_status_get(void)
{
    return ws_br_handler.eth_connection_ready;
}

bool ws_br_conn_handler_ws_connection_status_get(void)
{
    return ws_br_handler.ws_connection_ready;
}

void wisun_br_conn_handler_eth_interface_id_set(int8_t interfaceId)
{
    ws_br_handler.eth_interface_id = interfaceId;
    if (ws_br_handler.ws_interface_id > -1 &&
  		ws_br_handler.eth_interface_id > -1) {
    	ws_bbr_start(ws_br_handler.ws_interface_id, ws_br_handler.eth_interface_id);
    }
}

int8_t wisun_br_conn_handler_eth_interface_id_get(void)
{
    return ws_br_handler.eth_interface_id;
}
