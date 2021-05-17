/*
 * Copyright (c) 2016-2018, Pelion and affiliates.
 */

#include <string.h>
#include "net_interface.h"
#include "mbed-trace/mbed_trace.h"
#include "thread_bbr_api.h"
#include "thread_border_router_api.h"
#include "thread_br_conn_handler.h"
#include "thread_dhcpv6_server.h"
#include "borderrouter_helpers.h"
#include "common_functions.h"
#include "eventOS_event_timer.h"
#include "thread_bbr_ext.h"
#include "multicast_api.h"

#define TRACE_GROUP "TBRH"

typedef struct {

    int8_t  thread_interface_id;
    int8_t  eth_interface_id;
    bool    eth_connection_ready;
    bool    thread_connection_ready;
} thread_br_handler_t;

static thread_br_handler_t thread_br_handler;

void thread_br_conn_handler_init(void)
{
    thread_br_handler.thread_interface_id = -1;
    thread_br_handler.thread_connection_ready = 0;

    thread_br_handler.eth_interface_id = -1;
    thread_br_handler.eth_connection_ready = 0;
}

void thread_br_conn_handler_thread_connection_update(bool status)
{
    thread_br_handler.thread_connection_ready = status;
    tr_debug("mesh0 connection status: %d", status);

}

void thread_br_conn_handler_ethernet_connection_update(bool status)
{
    thread_br_handler.eth_connection_ready = status;
    tr_debug("Eth0 connection status: %d", status);

}

void thread_br_conn_handler_thread_interface_id_set(int8_t interfaceId)
{
    thread_br_handler.thread_interface_id = interfaceId;
    thread_bbr_extension_mesh_interface_updated_ntf(thread_br_handler.thread_interface_id);
    if (thread_br_handler.thread_interface_id > -1 && thread_br_handler.eth_interface_id > -1) {
        thread_bbr_start(thread_br_handler.thread_interface_id, thread_br_handler.eth_interface_id);
    }
}

int8_t thread_br_conn_handler_thread_interface_id_get(void)
{
    return thread_br_handler.thread_interface_id;
}

bool thread_br_conn_handler_eth_connection_status_get(void)
{
    return thread_br_handler.eth_connection_ready;
}

bool thread_br_conn_handler_thread_connection_status_get(void)
{
    return thread_br_handler.thread_connection_ready;
}

void thread_br_conn_handler_eth_interface_id_set(int8_t interfaceId)
{
    thread_br_handler.eth_interface_id = interfaceId;
    thread_bbr_extension_bb_interface_updated_ntf(thread_br_handler.eth_interface_id);
    if (thread_br_handler.thread_interface_id > -1 && thread_br_handler.eth_interface_id > -1) {
        thread_bbr_start(thread_br_handler.thread_interface_id, thread_br_handler.eth_interface_id);
    }
}

int8_t thread_br_conn_handler_eth_interface_id_get(void)
{
    return thread_br_handler.eth_interface_id;
}
