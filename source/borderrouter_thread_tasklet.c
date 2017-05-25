/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#define LOWPAN_ND 0
#define THREAD 1
#if MBED_CONF_APP_MESH_MODE == THREAD

#include <string.h>
#include <stdlib.h>
#include <mbed_assert.h>
#include "ns_types.h"
#include "eventOS_event.h"
#include "eventOS_event_timer.h"
#include "eventOS_scheduler.h"
#include "platform/arm_hal_timer.h"
#include "borderrouter_tasklet.h"
#include "borderrouter_helpers.h"
#include "net_interface.h"
#include "rf_wrapper.h"
#include "nwk_stats_api.h"
#include "ip6string.h"
#include "ethernet_mac_api.h"
#include "mac_api.h"
#include "sw_mac.h"
#include "mbed_interface.h"
#include "common_functions.h"
#include "thread_management_if.h"
#include "thread_br_conn_handler.h"
#include "randLIB.h"
#include "nsdynmemLIB.h"

#include "ns_trace.h"
#define TRACE_GROUP "brro"

#define NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY 2
#define NR_BACKHAUL_INTERFACE_PHY_DOWN  3
#define MESH_LINK_TIMEOUT 100
#define MESH_METRIC 1000
#define THREAD_MAX_CHILD_COUNT 32

static mac_api_t *api;
static eth_mac_api_t *eth_mac_api;

typedef enum {
    STATE_UNKNOWN,
    STATE_DISCONNECTED,
    STATE_LINK_READY,
    STATE_BOOTSTRAP,
    STATE_CONNECTED,
    STATE_MAX_VALUE
} connection_state_e;

typedef struct {
    int8_t prefix_len;
    uint8_t prefix[16];
    uint8_t next_hop[16];
} route_info_t;

/* Border router channel list */
static channel_list_s channel_list;

/* Backhaul prefix */
static uint8_t backhaul_prefix[16] = {0};

/* Backhaul default route information */
static route_info_t backhaul_route;
static int8_t br_tasklet_id = -1;

/* Network statistics */
static nwk_stats_t nwk_stats;

/* Function forward declarations */

static void thread_link_configuration_get(link_configuration_s *link_configuration);
static void network_interface_event_handler(arm_event_s *event);
static void mesh_network_up(void);
static void eth_network_data_init(void);
static net_ipv6_mode_e backhaul_bootstrap_mode = NET_IPV6_BOOTSTRAP_STATIC;
static void borderrouter_tasklet(arm_event_s *event);

static void print_interface_addr(int id)
{
    uint8_t address_buf[128];
    int address_count = 0;
    char buf[128];

    if (arm_net_address_list_get(id, 128, address_buf, &address_count) == 0) {
        uint8_t *t_buf = address_buf;
        for (int i = 0; i < address_count; ++i) {
            ip6tos(t_buf, buf);
            tr_info(" [%d] %s", i, buf);
            t_buf += 16;
        }
    }
}

static void eth_network_data_init()
{
    memset(&backhaul_prefix[8], 0, 8);

    /* Bootstrap mode for the backhaul interface */
#if MBED_CONF_APP_BACKHAUL_DYNAMIC_BOOTSTRAP == 1
    backhaul_bootstrap_mode = NET_IPV6_BOOTSTRAP_AUTONOMOUS;
    tr_info("NET_IPV6_BOOTSTRAP_AUTONOMOUS");

#else
    tr_info("NET_IPV6_BOOTSTRAP_STATIC");
    backhaul_bootstrap_mode = NET_IPV6_BOOTSTRAP_STATIC;
    // done like this so that prefix can be left out in the dynamic case.
    const char *param = MBED_CONF_APP_BACKHAUL_PREFIX;
    stoip6(param, strlen(param), backhaul_prefix);
    tr_info("backhaul_prefix: %s", print_ipv6(backhaul_prefix));

    /* Backhaul route configuration*/
    memset(&backhaul_route, 0, sizeof(backhaul_route));
#ifdef MBED_CONF_APP_BACKHAUL_NEXT_HOP
    param = MBED_CONF_APP_BACKHAUL_NEXT_HOP;
    stoip6(param, strlen(param), backhaul_route.next_hop);
    tr_info("next hop: %s", print_ipv6(backhaul_route.next_hop));
#endif
    param = MBED_CONF_APP_BACKHAUL_DEFAULT_ROUTE;
    char *prefix, route_buf[255] = {0};
    /* copy the config value to a non-const buffer */
    strncpy(route_buf, param, sizeof(route_buf) - 1);
    prefix = strtok(route_buf, "/");
    backhaul_route.prefix_len = atoi(strtok(NULL, "/"));
    stoip6(prefix, strlen(prefix), backhaul_route.prefix);
    tr_info("backhaul route prefix: %s", print_ipv6(backhaul_route.prefix));
#endif
}

static int thread_interface_up(void)
{
    int32_t val;
    device_configuration_s device_config;
    link_configuration_s link_setup;
    int8_t thread_if_id = thread_br_conn_handler_thread_interface_id_get();

    tr_info("thread_interface_up");
    memset(&device_config, 0, sizeof(device_config));

    const char *param = MBED_CONF_APP_PSKD;
    uint16_t len = strlen(param);
    MBED_ASSERT(len > 5 && len < 33);

    device_config.PSKd_ptr = malloc(len + 1);
    device_config.PSKd_len = len;
    memset(device_config.PSKd_ptr, 0, len + 1);
    memcpy(device_config.PSKd_ptr, param, len);

    thread_link_configuration_get(&link_setup);

    val = thread_management_node_init(thread_if_id, &channel_list, &device_config, &link_setup);

    if (val) {
        tr_error("Thread init error with code: %is\r\n", (int)val);
        return val;
    }

    // Additional thread configurations
    thread_management_set_link_timeout(thread_if_id, MESH_LINK_TIMEOUT);
    thread_management_max_child_count(thread_if_id, THREAD_MAX_CHILD_COUNT);

    val = arm_nwk_interface_up(thread_if_id);
    if (val != 0) {
        tr_error("mesh0 up Fail with code: %i\r\n", (int)val);
        return -1;
    } else {
        tr_info("mesh0 bootstrap ongoing...");
    }
    return 0;
}

static void thread_link_configuration_get(link_configuration_s *link_configuration)
{
    memset(link_configuration, 0, sizeof(link_configuration_s));

    MBED_ASSERT(strlen(MBED_CONF_APP_NETWORK_NAME) > 0 && strlen(MBED_CONF_APP_NETWORK_NAME) < 17);
    const uint8_t master_key[] = MBED_CONF_APP_THREAD_MASTER_KEY;
    MBED_ASSERT(sizeof(master_key) == 16);
    const uint8_t pskc[] = MBED_CONF_APP_PSKC;
    MBED_ASSERT(sizeof(pskc) == 16);

    const uint8_t extented_panid[] = MBED_CONF_APP_EXTENDED_PAN_ID;
    MBED_ASSERT(sizeof(extented_panid) == 8);
    const uint8_t mesh_local_prefix[] = MBED_CONF_APP_MESH_LOCAL_PREFIX;
    MBED_ASSERT(sizeof(mesh_local_prefix) == 8);

    memcpy(link_configuration->extented_pan_id, extented_panid, sizeof(extented_panid));
    memcpy(link_configuration->mesh_local_ula_prefix, mesh_local_prefix, sizeof(mesh_local_prefix));

    link_configuration->panId = MBED_CONF_APP_PAN_ID;
    tr_info("PAN ID %x", link_configuration->panId);
    memcpy(link_configuration->name, MBED_CONF_APP_NETWORK_NAME, strlen(MBED_CONF_APP_NETWORK_NAME));
    link_configuration->timestamp = MBED_CONF_APP_COMMISSIONING_DATASET_TIMESTAMP;

    memcpy(link_configuration->PSKc, pskc, sizeof(pskc));
    memcpy(link_configuration->master_key, master_key, sizeof(master_key));
    link_configuration->securityPolicy = SECURITY_POLICY_ALL_SECURITY;

    link_configuration->rfChannel = MBED_CONF_APP_RF_CHANNEL;
    tr_info("RF channel %d", link_configuration->rfChannel);
    link_configuration->channel_page = MBED_CONF_APP_RF_CHANNEL_PAGE;
    uint32_t channel_mask = MBED_CONF_APP_RF_CHANNEL_MASK;
    common_write_32_bit(channel_mask, link_configuration->channel_mask);

    link_configuration->key_rotation = 3600;
    link_configuration->key_sequence = 0;
}

// ethernet interface
static void network_interface_event_handler(arm_event_s *event)
{
    bool connectStatus = false;
    arm_nwk_interface_status_type_e status = (arm_nwk_interface_status_type_e)event->event_data;
    switch (status) {
        case (ARM_NWK_BOOTSTRAP_READY): { // Interface configured Bootstrap is ready

            connectStatus = true;
            tr_info("BR interface_id: %d", thread_br_conn_handler_eth_interface_id_get());
            if (-1 != thread_br_conn_handler_eth_interface_id_get()) {
                // metric set to high priority
                if (0 != arm_net_interface_set_metric(thread_br_conn_handler_eth_interface_id_get(), 0)) {
                    tr_warn("Failed to set metric for eth0.");
                }

                if (backhaul_bootstrap_mode == NET_IPV6_BOOTSTRAP_STATIC) {
                    uint8_t *next_hop_ptr;

                    if (memcmp(backhaul_route.next_hop, (const uint8_t[16]) {0}, 16) == 0) {
                        next_hop_ptr = NULL;
                    }
                    else {
                        next_hop_ptr = backhaul_route.next_hop;
                    }
                    tr_debug("Default route prefix: %s/%d", print_ipv6(backhaul_route.prefix),
                             backhaul_route.prefix_len);
                    tr_debug("Default route next hop: %s", print_ipv6(backhaul_route.next_hop));
                    arm_net_route_add(backhaul_route.prefix,
                                      backhaul_route.prefix_len,
                                      next_hop_ptr, 0xffffffff, 128,
                                      thread_br_conn_handler_eth_interface_id_get());
                }
                thread_br_conn_handler_eth_ready();
                tr_info("Backhaul interface addresses:");
                print_interface_addr(thread_br_conn_handler_eth_interface_id_get());
                thread_br_conn_handler_ethernet_connection_update(connectStatus);
            }
            break;
        }
        case (ARM_NWK_RPL_INSTANCE_FLOODING_READY): // RPL instance have been flooded
            tr_info("\rRPL instance have been flooded\r\n");
            break;
        case (ARM_NWK_SET_DOWN_COMPLETE): // Interface DOWN command successfully
            break;
        case (ARM_NWK_NWK_SCAN_FAIL):   // Interface have not detect any valid network
            tr_warning("\rmesh0 haven't detect any valid nw\r\n");
            break;
        case (ARM_NWK_IP_ADDRESS_ALLOCATION_FAIL): // IP address allocation fail(ND, DHCPv4 or DHCPv6)
            tr_error("\rNO GP address detected");
            break;
        case (ARM_NWK_DUPLICATE_ADDRESS_DETECTED): // User specific GP16 was not valid
            tr_error("\rEthernet IPv6 Duplicate addr detected!\r\n");
            break;
        case (ARM_NWK_AUHTENTICATION_START_FAIL): // No valid Authentication server detected behind access point ;
            tr_error("\rNo valid ath server detected behind AP\r\n");
            break;
        case (ARM_NWK_AUHTENTICATION_FAIL): // Network authentication fail by Handshake
            tr_error("\rNetwork authentication fail");
            break;
        case (ARM_NWK_NWK_CONNECTION_DOWN): // No connection between Access point or Default Router
            tr_warning("\rPrefix timeout\r\n");
            break;
        case (ARM_NWK_NWK_PARENT_POLL_FAIL): // Sleepy host poll fail 3 time
            tr_warning("\rParent poll fail\r\n");
            break;
        case (ARM_NWK_PHY_CONNECTION_DOWN): // Interface PHY cable off or serial port interface not respond anymore
            tr_error("\reth0 down\r\n");
            break;
        default:
            tr_warning("\rUnkown nw if event (type: %02x, id: %02x, data: %02x)\r\n", event->event_type, event->event_id, (unsigned int)event->event_data);
            break;
    }
}

void thread_interface_event_handler(arm_event_s *event)
{
    bool connectStatus = false;
    arm_nwk_interface_status_type_e status = (arm_nwk_interface_status_type_e)event->event_data;
    switch (status) {
        case (ARM_NWK_BOOTSTRAP_READY): { // Interface configured Bootstrap is ready
            connectStatus = true;
            tr_info("Thread bootstrap ready");

            if (arm_net_interface_set_metric(thread_br_conn_handler_thread_interface_id_get(), MESH_METRIC) != 0) {
                tr_warn("Failed to set metric for mesh0.");
            }

            tr_info("RF interface addresses:");
            print_interface_addr(thread_br_conn_handler_thread_interface_id_get());

            break;
        }
        case (ARM_NWK_SET_DOWN_COMPLETE):
            tr_info("Thread interface down");
            break;
        default:
            tr_warning("Unkown nw if event (type: %02x, id: %02x, data: %02x)\r\n", event->event_type, event->event_id, (unsigned int)event->event_data);
            break;
    }

    thread_br_conn_handler_thread_connection_update(connectStatus);
}

static void mesh_network_up()
{
    tr_debug("Create Mesh Interface");

    int status;
    int8_t thread_if_id;

    thread_if_id = arm_nwk_interface_lowpan_init(api, "ThreadInterface");
    tr_info("thread_if_id: %d", thread_if_id);
    MBED_ASSERT(thread_if_id>=0);

    if (thread_if_id < 0) {
        tr_error("arm_nwk_interface_lowpan_init() failed");
        return;
    }

    status = arm_nwk_interface_configure_6lowpan_bootstrap_set(
                thread_if_id,
                NET_6LOWPAN_ROUTER,
                NET_6LOWPAN_THREAD);

    if (status < 0) {
        tr_error("arm_nwk_interface_configure_6lowpan_bootstrap_set() failed");
        return;
    }

    thread_br_conn_handler_thread_interface_id_set(thread_if_id);

    status = thread_interface_up();
    MBED_ASSERT(!status);
    if (status) {
        tr_error("thread_interface_up() failed: %d", status);
    }
}

void thread_rf_init()
{
    mac_description_storage_size_t storage_sizes;
    storage_sizes.key_lookup_size = 1;
    storage_sizes.key_usage_size = 1;
    storage_sizes.device_decription_table_size = 32;
    storage_sizes.key_description_table_size = 6;

    int8_t rf_driver_id = rf_device_register();
    MBED_ASSERT(rf_driver_id>=0);
    if (rf_driver_id>=0) {
        randLIB_seed_random();

        if (!api) {
            api = ns_sw_mac_create(rf_driver_id, &storage_sizes);
        }
    }
}

void border_router_tasklet_start(void)
{
    net_init_core();
    thread_rf_init();
    protocol_stats_start(&nwk_stats);

    eventOS_event_handler_create(
        &borderrouter_tasklet,
        ARM_LIB_TASKLET_INIT_EVENT);
}


static void borderrouter_backhaul_phy_status_cb(uint8_t link_up, int8_t driver_id)
{
    arm_event_s event = {
        .sender = br_tasklet_id,
        .receiver = br_tasklet_id,
        .priority = ARM_LIB_MED_PRIORITY_EVENT,
        .event_type = APPLICATION_EVENT,
        .event_data = driver_id
    };

    if (link_up) {
        event.event_id = NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY;
    } else {
        event.event_id = NR_BACKHAUL_INTERFACE_PHY_DOWN;
    }

    tr_debug("Backhaul driver ID: %d", driver_id);

    eventOS_event_send(&event);
}

static int backhaul_interface_up(int8_t driver_id)
{
    int retval = -1;
    tr_debug("backhaul_interface_up: %i\n", driver_id);
    int8_t backhaul_if_id = thread_br_conn_handler_eth_interface_id_get();
    if (backhaul_if_id != -1) {
        tr_debug("Border RouterInterface already at active state\n");
    } else {

        if (!eth_mac_api) {
            eth_mac_api = ethernet_mac_create(driver_id);
        }

        backhaul_if_id = arm_nwk_interface_ethernet_init(eth_mac_api, "bh0");

        MBED_ASSERT(backhaul_if_id >= 0);
        if (backhaul_if_id >= 0) {
            tr_debug("Backhaul interface ID: %d", backhaul_if_id);
            thread_br_conn_handler_eth_interface_id_set(backhaul_if_id);
            arm_nwk_interface_configure_ipv6_bootstrap_set(
                backhaul_if_id, backhaul_bootstrap_mode, backhaul_prefix);
            arm_nwk_interface_up(backhaul_if_id);
            retval = 0;
        }
        else {
            tr_debug("Could not init ethernet");
        }
    }
    return retval;
}

static int backhaul_interface_down(void)
{
    int retval = -1;
    if (thread_br_conn_handler_eth_interface_id_get() != -1) {
        arm_nwk_interface_down(thread_br_conn_handler_eth_interface_id_get());
        thread_br_conn_handler_eth_interface_id_set(-1);
        thread_br_conn_handler_ethernet_connection_update(false);
        retval = 0;
    }
    else {
        tr_debug("Could not set eth down");
    }
    return retval;
}

#ifdef MBED_CONF_APP_DEBUG_TRACE
#if MBED_CONF_APP_DEBUG_TRACE == 1
static void memory_stat_print(void)
{
    const mem_stat_t *heap_info = ns_dyn_mem_get_mem_stat();
    if (heap_info) {
        tr_info(
            "Heap size: %" PRIu16 ", "
            "Reserved: %" PRIu16 ", "
            "Reserved max: %" PRIu16 ", "
            "Alloc fail: %" PRIu32 ""
            ,heap_info->heap_sector_size
            ,heap_info->heap_sector_allocated_bytes
            ,heap_info->heap_sector_allocated_bytes_max
            ,heap_info->heap_alloc_fail_cnt);
    }
}
#endif
#endif

/**
  * \brief Border Router Main Tasklet
  *
  *  Tasklet Handle next items:
  *
  *  - EV_INIT event: Set Certificate Chain, RF Interface Boot UP, multicast Init
  *  - SYSTEM_TIMER event: For RF interface Handshake purpose
  *
  */
static void borderrouter_tasklet(arm_event_s *event)
{
    arm_library_event_type_e event_type;
    event_type = (arm_library_event_type_e)event->event_type;

    switch (event_type) {
        case ARM_LIB_NWK_INTERFACE_EVENT:

            if (event->event_id == thread_br_conn_handler_eth_interface_id_get()) {
                network_interface_event_handler(event);
            } else {
                thread_interface_event_handler(event);
            }

            break;
        // comes from the backhaul_driver_init.
        case APPLICATION_EVENT:
            if (event->event_id == NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY) {
                int8_t net_backhaul_id = (int8_t) event->event_data;

                if (backhaul_interface_up(net_backhaul_id) != 0) {
                    tr_debug("Backhaul bootstrap start failed");
                } else {
                    tr_debug("Backhaul bootstrap started");
                }
            } else if (event->event_id == NR_BACKHAUL_INTERFACE_PHY_DOWN) {
                if (backhaul_interface_down() != 0) {
                    // may happend when booting first time.
                    tr_warning("Backhaul interface down failed");
                } else {
                    tr_debug("Backhaul interface is down");
                }
            }
            break;

        case ARM_LIB_TASKLET_INIT_EVENT:
            print_appl_info();
            br_tasklet_id = event->receiver;
            thread_br_conn_handler_init();
            eth_network_data_init();
            backhaul_driver_init(borderrouter_backhaul_phy_status_cb);
            mesh_network_up();
            eventOS_event_timer_request(9, ARM_LIB_SYSTEM_TIMER_EVENT, br_tasklet_id, 20000);
            break;

        case ARM_LIB_SYSTEM_TIMER_EVENT:
            eventOS_event_timer_cancel(event->event_id, event->receiver);

            if (event->event_id == 9) {
#ifdef MBED_CONF_APP_DEBUG_TRACE
#if MBED_CONF_APP_DEBUG_TRACE == 1
                arm_print_routing_table();
                arm_print_neigh_cache();
                memory_stat_print();
#endif
#endif
                eventOS_event_timer_request(9, ARM_LIB_SYSTEM_TIMER_EVENT, br_tasklet_id, 20000);
            }
            break;

        default:
            break;
    }
}

#endif // MBED_CONF_APP_MESH_MODE

