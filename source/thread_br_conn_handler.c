/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#include <string.h>
#include "net_interface.h"
#include "mbed-trace/mbed_trace.h"
#include "thread_border_router_api.h"
#include "thread_br_conn_handler.h"
#include "thread_dhcpv6_server.h"
#include "borderrouter_helpers.h"
#include "common_functions.h"
#include "eventOS_event_timer.h"
#include "thread_bbr_ext.h"
#include "multicast_api.h"

#define TRACE_GROUP "TBRH"
#define DHCP_SERVER_SHUTDOWN_TIMEOUT (100)

typedef struct {
    
    uint8_t dhcp_prefix[16];
    timeout_t *thread_dhcp_shutdown_timer;
    uint8_t dhcp_prefix_len;
    int8_t  thread_interface_id;
    int8_t  eth_interface_id;
    bool    eth_connection_ready;
    bool    thread_connection_ready;
    bool    dhcp_server_running;
} thread_br_handler_t;

static thread_br_handler_t thread_br_handler;
static void thread_br_conn_handler_border_router_startup_attempt(void);
static bool thread_br_conn_handler_default_route_enable(void);
static void thread_br_conn_handler_dhcp_server_stop_cb(void *arg);
static void thread_br_conn_handler_border_router_shutdown_request(void);


void thread_br_conn_handler_init(void)
{
    thread_br_handler.eth_connection_ready = false;
    thread_br_handler.thread_connection_ready = false;    
    thread_br_handler.dhcp_server_running = false;
    thread_br_handler.dhcp_prefix_len = 0;
    thread_br_handler.thread_interface_id = -1;
    thread_br_handler.eth_interface_id = -1;    
    thread_br_handler.thread_dhcp_shutdown_timer = NULL;
    memset(thread_br_handler.dhcp_prefix, 0, 16);
}

static void thread_br_conn_handler_border_router_startup_attempt(void)
{
    if (thread_br_handler.thread_dhcp_shutdown_timer != NULL) {
        tr_debug("DHCP server already running, enable default_route");
        eventOS_timeout_cancel(thread_br_handler.thread_dhcp_shutdown_timer);
        thread_br_handler.thread_dhcp_shutdown_timer = NULL;
    }

    if (!thread_br_handler.eth_connection_ready) {
        tr_debug("eth0 is down");
        return;
    } else if (!thread_br_handler.thread_connection_ready) {
        tr_debug("mesh0 is down");
        return;
    }

    if (thread_br_handler.dhcp_prefix_len == 0) {
        //No prefix/prefix_len to start DHCP server
        tr_error("DHCP server prefix length = 0");
        return;
    }

    if (thread_br_handler.thread_interface_id == -1) {
        tr_error("Thread interface ID not set");
        return;
    }

    if (thread_br_handler.dhcp_server_running == true) {
        // DHCP server is already running, enable default route
        tr_debug("DHCP server already running, enable default_route");
        thread_br_conn_handler_default_route_enable();
        return;
    }

    int retcode = thread_dhcpv6_server_add(thread_br_handler.thread_interface_id, thread_br_handler.dhcp_prefix, 200, true);
    if (retcode == 0) {
        tr_debug("DHCP server started ");
        if (thread_br_conn_handler_default_route_enable()) {
            thread_br_handler.dhcp_server_running = true;
            thread_bbr_extension_start(thread_br_handler.thread_interface_id, thread_br_handler.eth_interface_id);
        } else {
            tr_error("Failed to update DHCP default route");
        }
    } else {
        tr_error("DHCP server start failed");
    }
}

void thread_br_conn_handler_thread_connection_update(bool status)
{
    thread_br_handler.thread_connection_ready = status;
    tr_debug("mesh0 connection status: %d", status);

    if (status) {
        thread_br_conn_handler_border_router_startup_attempt();
    } else {
        // Thread network down. Reset DHCP server back to original state
        thread_br_handler.dhcp_server_running = false;
        // stop mDNS responder as no thread network
        thread_border_router_mdns_responder_stop();
        if (thread_br_handler.thread_dhcp_shutdown_timer != NULL) {
            // cancel active shutdown timer
            eventOS_timeout_cancel(thread_br_handler.thread_dhcp_shutdown_timer);
            thread_br_handler.thread_dhcp_shutdown_timer = NULL;
        }
    }
}

void thread_br_conn_handler_ethernet_connection_update(bool status)
{
    thread_br_handler.eth_connection_ready = status;
    tr_debug("Eth0 connection status: %d", status);

    if (status) {
        thread_br_conn_handler_border_router_startup_attempt();
    } else {
        // Ethernet connection down, request DHCP server shutdown
        thread_br_conn_handler_border_router_shutdown_request();
        thread_border_router_mdns_responder_stop();
    }

#if defined(MBED_CONF_APP_BACKHAUL_MLD)
    tr_debug("Configuring MLD proxying to upstream (interface_id = %d)", thread_br_handler.eth_interface_id);
    multicast_fwd_set_proxy_upstream(thread_br_handler.eth_interface_id);
#endif
}

void thread_br_conn_handler_eth_ready()
{    
    uint8_t prefix_len = 64;
    uint8_t global_address[16];
    
    if (0 == arm_net_address_get(thread_br_conn_handler_eth_interface_id_get(), ADDR_IPV6_GP, global_address)) {
        tr_info("Ethernet (eth0) bootstrap ready. IP: %s", print_ipv6(global_address));
    } else {
        tr_warn("arm_net_address_get fail");
    }
    thread_br_handler.dhcp_prefix_len = prefix_len;
    memset(thread_br_handler.dhcp_prefix, 0, 16);
    memcpy(thread_br_handler.dhcp_prefix, global_address, prefix_len / 8);
}

static bool thread_br_conn_handler_default_route_enable(void)
{
    thread_border_router_info_t thread_border_router_info;
    thread_border_router_info.Prf = 1;
    thread_border_router_info.P_preferred = false;
    thread_border_router_info.P_slaac = false;
    thread_border_router_info.P_dhcp = true;
    thread_border_router_info.P_configure = false;
    thread_border_router_info.P_default_route = true;
    thread_border_router_info.P_on_mesh = false;
    thread_border_router_info.P_nd_dns = false;
    thread_border_router_info.stableData = true;

    if (thread_border_router_prefix_add(thread_br_handler.thread_interface_id, thread_br_handler.dhcp_prefix, thread_br_handler.dhcp_prefix_len, &thread_border_router_info) == 0) {
        thread_border_router_publish(thread_br_handler.thread_interface_id);
        tr_debug("Updated %s prefix", print_ipv6_prefix(thread_br_handler.dhcp_prefix, thread_br_handler.dhcp_prefix_len));
        thread_border_router_mdns_responder_start(thread_br_handler.thread_interface_id, thread_br_handler.eth_interface_id, "ARM-BR");
        return true;
    } else {
        tr_error("Failed to enable default_route flag to prefix");
        return false;
    }
}

static void thread_br_conn_handler_dhcp_server_stop_cb(void *arg)
{
    (void)arg;

    tr_debug("DHCP server stop cb");
    thread_br_handler.thread_dhcp_shutdown_timer = NULL;
    thread_br_handler.dhcp_server_running = false;
    thread_dhcpv6_server_delete(thread_br_handler.thread_interface_id, thread_br_handler.dhcp_prefix);
    thread_border_router_publish(thread_br_handler.thread_interface_id);
    thread_br_handler.dhcp_prefix_len = 0;
    memset(thread_br_handler.dhcp_prefix, 0, 16);
}

static void thread_br_conn_handler_border_router_shutdown_request(void)
{
    if (thread_br_handler.dhcp_server_running && thread_br_handler.thread_dhcp_shutdown_timer == NULL) {
        tr_debug("DHCP server shutdown timer started");
        thread_br_handler.thread_dhcp_shutdown_timer = eventOS_timeout_ms(thread_br_conn_handler_dhcp_server_stop_cb, DHCP_SERVER_SHUTDOWN_TIMEOUT, NULL);
    }
}

void thread_br_conn_handler_thread_interface_id_set(int8_t interfaceId)
{
    thread_br_handler.thread_interface_id = interfaceId;
    thread_bbr_extension_mesh_interface_updated_ntf(thread_br_handler.thread_interface_id);
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
}

int8_t thread_br_conn_handler_eth_interface_id_get(void)
{
    return thread_br_handler.eth_interface_id;
}
