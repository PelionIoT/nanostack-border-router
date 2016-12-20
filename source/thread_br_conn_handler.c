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


#define TRACE_GROUP "ThBrConnHandl"
#define DHCP_SERVER_SHUTDOWN_TIMEOUT (100)

typedef struct thread_interface_status_s {
    
    bool    ethConnectionReady;  //eth0 status
    bool    threadNetworkConnectionReady;  //mesh0 status    
    timeout_t *threadDhcpShutdownTimer;
    bool    dhcpServerRunning;
    uint8_t dhcpPrefix[16];
    uint8_t dhcpPrefixLen;
    int8_t  threadInterfaceId;
    int8_t  borderRouterInterfaceId;
} thread_interface_status_s;

static thread_interface_status_s thread_interface_borderrouter_status;
static void thread_br_conn_handler_border_router_startup_attempt(void);
static bool thread_br_conn_handler_default_route_enable(void);
static void thread_br_conn_handler_dhcp_server_stop_cb(void *arg);
static void thread_br_conn_handler_border_router_shutdown_request(void);


void thread_br_conn_handler_init(void)
{
    thread_interface_borderrouter_status.ethConnectionReady = false;
    thread_interface_borderrouter_status.threadNetworkConnectionReady = false;    
    thread_interface_borderrouter_status.dhcpServerRunning = false;
    thread_interface_borderrouter_status.dhcpPrefixLen = 0;
    thread_interface_borderrouter_status.threadInterfaceId = -1;
    thread_interface_borderrouter_status.borderRouterInterfaceId = -1;    
    thread_interface_borderrouter_status.threadDhcpShutdownTimer = NULL;
    memset(thread_interface_borderrouter_status.dhcpPrefix, 0, 16);
}

static void thread_br_conn_handler_border_router_startup_attempt(void)
{
    if (thread_interface_borderrouter_status.threadDhcpShutdownTimer != NULL) {
        tr_debug("DHCP server already running, enable default_route");
        eventOS_timeout_cancel(thread_interface_borderrouter_status.threadDhcpShutdownTimer);
        thread_interface_borderrouter_status.threadDhcpShutdownTimer = NULL;
    }

    if (!thread_interface_borderrouter_status.ethConnectionReady) {
        tr_debug("eth0 is down");
        return;
    } else if (!thread_interface_borderrouter_status.threadNetworkConnectionReady) {
        tr_debug("mesh0 is down");
        return;
    }

    if (thread_interface_borderrouter_status.dhcpPrefixLen == 0) {
        //No prefix/prefix_len to start DHCP server
        tr_error("DHCP server prefix length = 0");
        return;
    }

    if (thread_interface_borderrouter_status.threadInterfaceId == -1) {
        tr_error("Thread interface ID not set");
        return;
    }

    if (thread_interface_borderrouter_status.dhcpServerRunning == true) {
        // DHCP server is already running, enable default route
        tr_debug("DHCP server already running, enable default_route");
        thread_br_conn_handler_default_route_enable();
        return;
    }

    int retcode = thread_dhcpv6_server_add(thread_interface_borderrouter_status.threadInterfaceId, thread_interface_borderrouter_status.dhcpPrefix, 200, true);
    if (retcode == 0) {
        tr_debug("DHCP server started ");
        if (thread_br_conn_handler_default_route_enable()) {
            thread_interface_borderrouter_status.dhcpServerRunning = true;
        } else {
            tr_error("Failed to update DHCP default route");
        }
    } else {
        tr_error("DHCP server start failed");
    }
}

void thread_br_conn_handler_update_thread_connection(bool status)
{
    thread_interface_borderrouter_status.threadNetworkConnectionReady = status;
    if (status) {
        tr_debug("mesh0 connected");
        thread_br_conn_handler_border_router_startup_attempt();
    } else {
        // Thread network down. Reset DHCP server back to original state
        thread_interface_borderrouter_status.dhcpServerRunning = false;
        if (thread_interface_borderrouter_status.threadDhcpShutdownTimer != NULL) {
            // cancel active shutdown timer
            eventOS_timeout_cancel(thread_interface_borderrouter_status.threadDhcpShutdownTimer);
            thread_interface_borderrouter_status.threadDhcpShutdownTimer = NULL;
        }
    }
}


void thread_br_conn_handler_update_ethernet_connection(bool status)
{

    thread_interface_borderrouter_status.ethConnectionReady = status;
    if (status) {
        tr_debug("Eth0 connected");
        thread_br_conn_handler_border_router_startup_attempt();
    } else {
        // Ethernet connection down, request DHCP server shutdown
        thread_br_conn_handler_border_router_shutdown_request();
        tr_debug("Eth0 disconnected");
    }
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
    thread_interface_borderrouter_status.dhcpPrefixLen = prefix_len;
    memset(thread_interface_borderrouter_status.dhcpPrefix, 0, 16);
    memcpy(thread_interface_borderrouter_status.dhcpPrefix, global_address, prefix_len / 8);
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

    if (thread_border_router_prefix_add(thread_interface_borderrouter_status.threadInterfaceId, thread_interface_borderrouter_status.dhcpPrefix, thread_interface_borderrouter_status.dhcpPrefixLen, &thread_border_router_info) == 0) {
        thread_border_router_publish(thread_interface_borderrouter_status.threadInterfaceId);
        tr_debug("Updated %s prefix", print_ipv6_prefix(thread_interface_borderrouter_status.dhcpPrefix, thread_interface_borderrouter_status.dhcpPrefixLen));
        return true;
    } else {
        tr_error("Failed to enable default_route flag to prefix");
        return false;
    }
}

static void thread_br_conn_handler_dhcp_server_stop_cb(void *arg)
{
    (void)arg;
    thread_interface_borderrouter_status.threadDhcpShutdownTimer = NULL;
    thread_interface_borderrouter_status.dhcpServerRunning = false;
    thread_dhcpv6_server_delete(thread_interface_borderrouter_status.threadInterfaceId, thread_interface_borderrouter_status.dhcpPrefix);
    thread_border_router_publish(thread_interface_borderrouter_status.threadInterfaceId);

    tr_debug("DHCP server stop cb");
    thread_interface_borderrouter_status.dhcpPrefixLen = 0;
    memset(thread_interface_borderrouter_status.dhcpPrefix, 0, 16);
}

static void thread_br_conn_handler_border_router_shutdown_request(void)
{
    if (thread_interface_borderrouter_status.dhcpServerRunning && thread_interface_borderrouter_status.threadDhcpShutdownTimer == NULL) {
        tr_debug("DHCP server shutdown timer started");
        thread_interface_borderrouter_status.threadDhcpShutdownTimer = eventOS_timeout_ms(thread_br_conn_handler_dhcp_server_stop_cb, DHCP_SERVER_SHUTDOWN_TIMEOUT, NULL);
    }
}

void thread_br_conn_handler_thread_interface_id_set(int8_t interfaceId)
{
    thread_interface_borderrouter_status.threadInterfaceId = interfaceId;
}

int8_t thread_br_conn_handler_thread_interface_id_get(void)
{
    return thread_interface_borderrouter_status.threadInterfaceId;
}

bool thread_br_conn_handler_eth_connection_status_get(void)
{
    return thread_interface_borderrouter_status.ethConnectionReady;
}

bool thread_br_conn_handler_thread_connection_status_get(void)
{
    return thread_interface_borderrouter_status.threadNetworkConnectionReady;
}

void thread_br_conn_handler_eth_interface_id_set(int8_t interfaceId)
{
    thread_interface_borderrouter_status.borderRouterInterfaceId = interfaceId;
}

int8_t thread_br_conn_handler_eth_interface_id_get(void)
{
    return thread_interface_borderrouter_status.borderRouterInterfaceId;
}
