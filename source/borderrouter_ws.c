/*
 * Copyright (c) 2019 ARM Limited. All rights reserved.
 */

#define LOWPAN_ND 0
#define THREAD 1
#define LOWPAN_WS 2
#if MBED_CONF_APP_MESH_MODE == LOWPAN_WS

#include <string.h>
#include <stdlib.h>
#include <mbed_assert.h>
#include "eventOS_event.h"
#include "eventOS_event_timer.h"
#include "eventOS_scheduler.h"
#include "platform/arm_hal_timer.h"
#include "borderrouter_tasklet.h"
#include "borderrouter_helpers.h"
#include "net_interface.h"
#include "rf_wrapper.h"
#include "fhss_api.h"
#include "fhss_config.h"
#include "ws_management_api.h"
#include "ws_bbr_api.h"
#include "ip6string.h"
#include "mac_api.h"
#include "ethernet_mac_api.h"
#include "sw_mac.h"
#include "nwk_stats_api.h"
#include "randLIB.h"
#ifdef MBED_CONF_APP_CERTIFICATE_HEADER
#include MBED_CONF_APP_CERTIFICATE_HEADER
#endif

#include "ns_trace.h"
#define TRACE_GROUP "brro"

#define NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY 2
#define NR_BACKHAUL_INTERFACE_PHY_DOWN  3
#define MESH_LINK_TIMEOUT 100
#define MESH_METRIC 1000

#define WS_DEFAULT_REGULATORY_DOMAIN 255
#define WS_DEFAULT_OPERATING_CLASS 255
#define WS_DEFAULT_OPERATING_MODE 255
#define WS_DEFAULT_UC_CHANNEL_FUNCTION 255
#define WS_DEFAULT_BC_CHANNEL_FUNCTION 255
#define WS_DEFAULT_UC_DWELL_INTERVAL 0
#define WS_DEFAULT_BC_INTERVAL 0
#define WS_DEFAULT_BC_DWELL_INTERVAL 0
#define WS_DEFAULT_UC_FIXED_CHANNEL 0xffff
#define WS_DEFAULT_BC_FIXED_CHANNEL 0xffff

static mac_api_t *mac_api;
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

typedef struct {
    int8_t  ws_interface_id;
    int8_t  net_interface_id;
} ws_br_handler_t;

static ws_br_handler_t ws_br_handler;

/* Backhaul prefix */
static uint8_t backhaul_prefix[16] = {0};

/* Backhaul default route information */
static route_info_t backhaul_route;
static int8_t br_tasklet_id = -1;

/* Network statistics */
static nwk_stats_t nwk_stats;

/* Function forward declarations */

static void network_interface_event_handler(arm_event_s *event);
static void mesh_network_up(void);
static void eth_network_data_init(void);
static net_ipv6_mode_e backhaul_bootstrap_mode = NET_IPV6_BOOTSTRAP_STATIC;
static void borderrouter_tasklet(arm_event_s *event);
static int wisun_interface_up(void);
static void wisun_interface_event_handler(arm_event_s *event);
static void network_interface_event_handler(arm_event_s *event);
static int backhaul_interface_down(void);
static void borderrouter_backhaul_phy_status_cb(uint8_t link_up, int8_t driver_id);
extern fhss_timer_t fhss_functions;

typedef struct {
    char *network_name;
    uint8_t regulatory_domain;
    uint8_t operating_class;
    uint8_t operating_mode;
    uint8_t uc_channel_function;
    uint8_t bc_channel_function;
    uint8_t uc_dwell_interval;
    uint32_t bc_interval;
    uint8_t bc_dwell_interval;
    uint16_t uc_fixed_channel;
    uint16_t bc_fixed_channel;
} ws_config_t;
static ws_config_t ws_conf;

static void mesh_network_up()
{
    tr_debug("Create Mesh Interface");

    int status;
    int8_t wisun_if_id = ws_br_handler.ws_interface_id;

    status = arm_nwk_interface_configure_6lowpan_bootstrap_set(
                 wisun_if_id,
                 NET_6LOWPAN_BORDER_ROUTER,
                 NET_6LOWPAN_WS);

    if (status < 0) {
        tr_error("arm_nwk_interface_configure_6lowpan_bootstrap_set() failed");
        return;
    }

    status = wisun_interface_up();
    MBED_ASSERT(!status);
    if (status) {
        tr_error("wisun_interface_up() failed: %d", status);
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

void load_config(void)
{
    ws_conf.network_name = malloc(sizeof(MBED_CONF_APP_NETWORK_NAME) + 1);
    strcpy(ws_conf.network_name, MBED_CONF_APP_NETWORK_NAME);
#ifdef MBED_CONF_APP_REGULATORY_DOMAIN
    ws_conf.regulatory_domain = MBED_CONF_APP_REGULATORY_DOMAIN;
#else
    ws_conf.regulatory_domain = WS_DEFAULT_REGULATORY_DOMAIN;
#endif //MBED_CONF_APP_REGULATORY_DOMAIN
#ifdef MBED_CONF_APP_OPERATING_CLASS
    ws_conf.operating_class = MBED_CONF_APP_OPERATING_CLASS;
#else
    ws_conf.operating_class = WS_DEFAULT_OPERATING_CLASS;
#endif //MBED_CONF_APP_OPERATING_CLASS
#ifdef MBED_CONF_APP_OPERATING_MODE
    ws_conf.operating_mode = MBED_CONF_APP_OPERATING_MODE;
#else
    ws_conf.operating_mode = WS_DEFAULT_OPERATING_MODE;
#endif //MBED_CONF_APP_OPERATING_MODE
#ifdef MBED_CONF_APP_UC_CHANNEL_FUNCTION
    ws_conf.uc_channel_function = MBED_CONF_APP_UC_CHANNEL_FUNCTION;
#else
    ws_conf.uc_channel_function = WS_DEFAULT_UC_CHANNEL_FUNCTION;
#endif //MBED_CONF_APP_UC_CHANNEL_FUNCTION
#ifdef MBED_CONF_APP_BC_CHANNEL_FUNCTION
    ws_conf.bc_channel_function = MBED_CONF_APP_BC_CHANNEL_FUNCTION;
#else
    ws_conf.bc_channel_function = WS_DEFAULT_BC_CHANNEL_FUNCTION;
#endif //MBED_CONF_APP_UC_CHANNEL_FUNCTION
#ifdef MBED_CONF_APP_UC_DWELL_INTERVAL
    ws_conf.uc_dwell_interval = MBED_CONF_APP_UC_DWELL_INTERVAL;
#else
    ws_conf.uc_dwell_interval = WS_DEFAULT_UC_DWELL_INTERVAL;
#endif //MBED_CONF_APP_UC_DWELL_INTERVAL
#ifdef MBED_CONF_APP_BC_INTERVAL
    ws_conf.bc_interval = MBED_CONF_APP_BC_INTERVAL;
#else
    ws_conf.bc_interval = WS_DEFAULT_BC_INTERVAL;
#endif //MBED_CONF_APP_BC_INTERVAL
#ifdef MBED_CONF_APP_BC_DWELL_INTERVAL
    ws_conf.bc_dwell_interval = MBED_CONF_APP_BC_DWELL_INTERVAL;
#else
    ws_conf.bc_dwell_interval = WS_DEFAULT_BC_DWELL_INTERVAL;
#endif //MBED_CONF_APP_BC_DWELL_INTERVAL
// Using randomized fixed channel by default
#ifdef MBED_CONF_APP_UC_FIXED_CHANNEL
    ws_conf.uc_fixed_channel = MBED_CONF_APP_UC_FIXED_CHANNEL;
#else
    ws_conf.uc_fixed_channel = WS_DEFAULT_UC_FIXED_CHANNEL;
#endif //MBED_CONF_APP_UC_FIXED_CHANNEL
#ifdef MBED_CONF_APP_BC_FIXED_CHANNEL
    ws_conf.bc_fixed_channel = MBED_CONF_APP_BC_FIXED_CHANNEL;
#else
    ws_conf.bc_fixed_channel = WS_DEFAULT_BC_FIXED_CHANNEL;
#endif //MBED_CONF_APP_BC_FIXED_CHANNEL
}

void wisun_rf_init()
{
    mac_description_storage_size_t storage_sizes;
    storage_sizes.device_decription_table_size = 32;
    storage_sizes.key_description_table_size = 4;
    storage_sizes.key_lookup_size = 1;
    storage_sizes.key_usage_size = 1;

    int8_t rf_driver_id = rf_device_register();
    MBED_ASSERT(rf_driver_id >= 0);
    if (rf_driver_id >= 0) {
        randLIB_seed_random();
        if (!mac_api) {
            mac_api = ns_sw_mac_create(rf_driver_id, &storage_sizes);
        }

        ws_br_handler.ws_interface_id = arm_nwk_interface_lowpan_init(mac_api, ws_conf.network_name);

        if (ws_br_handler.ws_interface_id < 0) {
            tr_error("Wi-SUN interface creation failed");
            return;
        }

        if (ws_br_handler.ws_interface_id > -1 &&
                ws_br_handler.net_interface_id > -1) {
            ws_bbr_start(ws_br_handler.ws_interface_id, ws_br_handler.net_interface_id);
        }
    }
}


static int wisun_interface_up(void)
{
    int32_t ret;

    fhss_timer_t *fhss_timer_ptr = NULL;

    fhss_timer_ptr = &fhss_functions;

    ret = ws_management_node_init(ws_br_handler.ws_interface_id, ws_conf.regulatory_domain, ws_conf.network_name, fhss_timer_ptr);
    if (0 != ret) {
        tr_error("WS node init fail - code %"PRIi32"", ret);
        return -1;
    }

    if (ws_conf.uc_channel_function != WS_DEFAULT_UC_CHANNEL_FUNCTION) {
        ret = ws_management_fhss_unicast_channel_function_configure(ws_br_handler.ws_interface_id, ws_conf.uc_channel_function, ws_conf.uc_fixed_channel, ws_conf.uc_dwell_interval);
        if (ret != 0) {
            tr_error("Unicast channel function configuration failed %"PRIi32"", ret);
            return -1;
        }
    }
    if (ws_conf.bc_channel_function != WS_DEFAULT_BC_CHANNEL_FUNCTION ||
            ws_conf.bc_dwell_interval != WS_DEFAULT_BC_DWELL_INTERVAL ||
            ws_conf.bc_interval != WS_DEFAULT_BC_INTERVAL) {
        ret = ws_management_fhss_broadcast_channel_function_configure(ws_br_handler.ws_interface_id, ws_conf.bc_channel_function, ws_conf.bc_fixed_channel, ws_conf.bc_dwell_interval, ws_conf.bc_interval);
        if (ret != 0) {
            tr_error("Broadcast channel function configuration failed %"PRIi32"", ret);
            return -1;
        }
    }

    if (ws_conf.uc_dwell_interval != WS_DEFAULT_UC_DWELL_INTERVAL ||
            ws_conf.bc_dwell_interval != WS_DEFAULT_BC_DWELL_INTERVAL ||
            ws_conf.bc_interval != WS_DEFAULT_BC_INTERVAL) {
        ret = ws_management_fhss_timing_configure(ws_br_handler.ws_interface_id, ws_conf.uc_dwell_interval, ws_conf.bc_interval, ws_conf.bc_dwell_interval);
        if (ret != 0) {
            tr_error("fhss configuration failed %"PRIi32"", ret);
            return -1;
        }
    }
    if (ws_conf.regulatory_domain != WS_DEFAULT_REGULATORY_DOMAIN ||
            ws_conf.operating_mode != WS_DEFAULT_OPERATING_MODE ||
            ws_conf.operating_class != WS_DEFAULT_OPERATING_CLASS) {
        ret = ws_management_regulatory_domain_set(ws_br_handler.ws_interface_id, ws_conf.regulatory_domain, ws_conf.operating_class, ws_conf.operating_mode);
        if (ret != 0) {
            tr_error("Regulatory domain configuration failed %"PRIi32"", ret);
            return -1;
        }
    }

#if defined(MBED_CONF_APP_CERTIFICATE_HEADER)
    arm_certificate_chain_entry_s chain_info;
    memset(&chain_info, 0, sizeof(arm_certificate_chain_entry_s));
    chain_info.cert_chain[0] = (const uint8_t *) MBED_CONF_APP_ROOT_CERTIFICATE;
    chain_info.cert_len[0] = strlen((const char *) MBED_CONF_APP_ROOT_CERTIFICATE) + 1;
    chain_info.cert_chain[1] = (const uint8_t *) MBED_CONF_APP_OWN_CERTIFICATE;
    chain_info.cert_len[1] = strlen((const char *) MBED_CONF_APP_OWN_CERTIFICATE) + 1;
    chain_info.key_chain[1] = (const uint8_t *) MBED_CONF_APP_OWN_CERTIFICATE_KEY;
    chain_info.chain_length = 2;
    arm_network_certificate_chain_set((const arm_certificate_chain_entry_s *) &chain_info);
#endif
    ret = arm_nwk_interface_up(ws_br_handler.ws_interface_id);
    if (ret != 0) {
        tr_error("mesh0 up Fail with code: %"PRIi32"", ret);
        return ret;
    }
    tr_info("mesh0 bootstrap ongoing..");
    return 0;
}

void border_router_tasklet_start(void)
{
    ws_br_handler.ws_interface_id = -1;
    ws_br_handler.net_interface_id = -1;

    load_config();
    wisun_rf_init();
    protocol_stats_start(&nwk_stats);

    eventOS_event_handler_create(
        &borderrouter_tasklet,
        ARM_LIB_TASKLET_INIT_EVENT);
}

#undef ETH
#undef SLIP
#undef EMAC
#undef CELL
#define ETH 1
#define SLIP 2
#define EMAC 3
#define CELL 4

static int backhaul_interface_up(int8_t driver_id)
{
    int retval = -1;
    tr_debug("backhaul_interface_up: %i", driver_id);
    if (ws_br_handler.net_interface_id != -1) {
        tr_debug("Border RouterInterface already at active state");
        return retval;
    }

    if (!eth_mac_api) {
        eth_mac_api = ethernet_mac_create(driver_id);
    }

#if MBED_CONF_APP_BACKHAUL_DRIVER == CELL
    if (eth_mac_api->iid64_get) {
        ws_br_handler.net_interface_id = arm_nwk_interface_ppp_init(eth_mac_api, "ppp0");
    } else
#endif
    {
        ws_br_handler.net_interface_id = arm_nwk_interface_ethernet_init(eth_mac_api, "bh0");
    }

    MBED_ASSERT(ws_br_handler.net_interface_id >= 0);
    if (ws_br_handler.net_interface_id >= 0) {
        tr_debug("Backhaul interface ID: %d", ws_br_handler.net_interface_id);
        if (ws_br_handler.ws_interface_id > -1) {
            ws_bbr_start(ws_br_handler.ws_interface_id, ws_br_handler.net_interface_id);
        }
        arm_nwk_interface_configure_ipv6_bootstrap_set(
            ws_br_handler.net_interface_id, backhaul_bootstrap_mode, backhaul_prefix);
        arm_nwk_interface_up(ws_br_handler.net_interface_id);
        retval = 0;
    } else {
        tr_error("Could not init ethernet");
    }

    return retval;
}

#undef ETH
#undef SLIP
#undef EMAC
#undef CELL

static int backhaul_interface_down(void)
{
    int retval = -1;
    if (ws_br_handler.net_interface_id != -1) {
        arm_nwk_interface_down(ws_br_handler.net_interface_id);
        ws_br_handler.net_interface_id = -1;
        retval = 0;
    } else {
        tr_debug("Could not set eth down");
    }
    return retval;
}

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

#if MBED_CONF_APP_DEBUG_TRACE
static void print_interface_addresses(void)
{
    tr_info("Backhaul interface addresses:");
    print_interface_addr(ws_br_handler.net_interface_id);

    tr_info("RF interface addresses:");
    print_interface_addr(ws_br_handler.ws_interface_id);
}
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

            if (event->event_id == ws_br_handler.net_interface_id) {
                network_interface_event_handler(event);
            } else {
                wisun_interface_event_handler(event);
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
                if (backhaul_interface_down() == 0) {
                    tr_debug("Backhaul interface is down");
                }
            }
            break;

        case ARM_LIB_TASKLET_INIT_EVENT:
            br_tasklet_id = event->receiver;
            eth_network_data_init();
            backhaul_driver_init(borderrouter_backhaul_phy_status_cb);
            mesh_network_up();
            eventOS_event_timer_request(9, ARM_LIB_SYSTEM_TIMER_EVENT, br_tasklet_id, 20000);
            break;

        case ARM_LIB_SYSTEM_TIMER_EVENT:
            eventOS_event_timer_cancel(event->event_id, event->receiver);

            if (event->event_id == 9) {
#if MBED_CONF_APP_DEBUG_TRACE
                arm_print_routing_table();
                arm_print_neigh_cache();
                print_memory_stats();
                // Trace interface addresses. This trace can be removed if nanostack prints added/removed
                // addresses.
                print_interface_addresses();
#endif
                eventOS_event_timer_request(9, ARM_LIB_SYSTEM_TIMER_EVENT, br_tasklet_id, 20000);
            }
            break;

        default:
            break;
    }
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

    eventOS_event_send(&event);
}

// ethernet interface
static void network_interface_event_handler(arm_event_s *event)
{
    arm_nwk_interface_status_type_e status = (arm_nwk_interface_status_type_e)event->event_data;
    switch (status) {
        case (ARM_NWK_BOOTSTRAP_READY): { // Interface configured Bootstrap is ready

            tr_info("BR interface_id: %d", ws_br_handler.net_interface_id);
            if (-1 != ws_br_handler.net_interface_id) {
                // metric set to high priority
                if (0 != arm_net_interface_set_metric(ws_br_handler.net_interface_id, 0)) {
                    tr_warn("Failed to set metric for eth0.");
                }

                if (backhaul_bootstrap_mode == NET_IPV6_BOOTSTRAP_STATIC) {
                    uint8_t *next_hop_ptr;

                    if (memcmp(backhaul_route.next_hop, (const uint8_t[16]) {
                    0
                }, 16) == 0) {
                        next_hop_ptr = NULL;
                    } else {
                        next_hop_ptr = backhaul_route.next_hop;
                    }
                    tr_debug("Default route prefix: %s/%d", print_ipv6(backhaul_route.prefix),
                             backhaul_route.prefix_len);
                    tr_debug("Default route next hop: %s", print_ipv6(backhaul_route.next_hop));
                    arm_net_route_add(backhaul_route.prefix,
                                      backhaul_route.prefix_len,
                                      next_hop_ptr, 0xffffffff, 128,
                                      ws_br_handler.net_interface_id);
                }
                tr_info("Backhaul interface addresses:");
                print_interface_addr(ws_br_handler.net_interface_id);
            }
            break;
        }
        case (ARM_NWK_RPL_INSTANCE_FLOODING_READY): // RPL instance have been flooded
            tr_info("RPL instance have been flooded");
            break;
        case (ARM_NWK_SET_DOWN_COMPLETE): // Interface DOWN command successfully
            break;
        case (ARM_NWK_NWK_SCAN_FAIL):   // Interface have not detect any valid network
            tr_warning("mesh0 haven't detect any valid nwk");
            break;
        case (ARM_NWK_IP_ADDRESS_ALLOCATION_FAIL): // IP address allocation fail(ND, DHCPv4 or DHCPv6)
            tr_error("NO GP address detected");
            break;
        case (ARM_NWK_DUPLICATE_ADDRESS_DETECTED): // User specific GP16 was not valid
            tr_error("Ethernet IPv6 Duplicate addr detected!");
            break;
        case (ARM_NWK_AUHTENTICATION_START_FAIL): // No valid Authentication server detected behind access point ;
            tr_error("No valid ath server detected behind AP");
            break;
        case (ARM_NWK_AUHTENTICATION_FAIL): // Network authentication fail by Handshake
            tr_error("Network authentication fail");
            break;
        case (ARM_NWK_NWK_CONNECTION_DOWN): // No connection between Access point or Default Router
            tr_warning("Prefix timeout");
            break;
        case (ARM_NWK_NWK_PARENT_POLL_FAIL): // Sleepy host poll fail 3 time
            tr_warning("Parent poll fail");
            break;
        case (ARM_NWK_PHY_CONNECTION_DOWN): // Interface PHY cable off or serial port interface not respond anymore
            tr_error("eth0 down");
            break;
        default:
            tr_warning("Unknown nwk if event (type: %02x, id: %02x, data: %02x)", event->event_type, event->event_id, (unsigned int)event->event_data);
            break;
    }
}

static void wisun_interface_event_handler(arm_event_s *event)
{
    arm_nwk_interface_status_type_e status = (arm_nwk_interface_status_type_e)event->event_data;
    switch (status) {
        case (ARM_NWK_BOOTSTRAP_READY): { // Interface configured Bootstrap is ready
            tr_info("Wisun bootstrap ready");

            if (arm_net_interface_set_metric(ws_br_handler.ws_interface_id, MESH_METRIC) != 0) {
                tr_warn("Failed to set metric for mesh0.");
            }

            tr_info("RF interface addresses:");
            print_interface_addr(ws_br_handler.ws_interface_id);

            break;
        }
        case (ARM_NWK_SET_DOWN_COMPLETE):
            tr_info("Wisun interface down");
            break;
        default:
            tr_warning("Unknown nwk if event (type: %02x, id: %02x, data: %02x)", event->event_type, event->event_id, (unsigned int)event->event_data);
            break;
    }

}

#endif
