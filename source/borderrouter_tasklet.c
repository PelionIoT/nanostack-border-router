/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdlib.h>
#include "ns_types.h"
#include "eventOS_event.h"
#include "eventOS_event_timer.h"
#include "eventOS_scheduler.h"
#include "multicast_api.h"
#include "whiteboard_api.h"
#include "platform/arm_hal_timer.h"
#include "nanostack-border-router/borderrouter_tasklet.h"
#include "nanostack-border-router/borderrouter_helpers.h"
#include "nanostack-border-router/cfg_parser.h"
#include "atmel-rf-driver/driverRFPhy.h"
#include "nwk_stats_api.h"
#include "net_interface.h"
#include "ip6string.h"
#include "net_rpl.h"
#include "mac_api.h"
#include "ethernet_mac_api.h"
#include "sw_mac.h"

#if defined(YOTTA_CFG_BORDER_ROUTER) || defined(MBED_CONF_APP_DEFINED_BR_CONFIG)
#include "nanostack-border-router/mbed_config.h"
#else
#warning No configuration provided: using empty config
static conf_t static_config[] = {
    {NULL, NULL, 0}
};
conf_t *global_config = static_config;
#endif

#if YOTTA_CFG_BORDER_ROUTER_DEBUG_TRACES==1
#define HAVE_DEBUG 1
#endif

#include "ns_trace.h"
#define TRACE_GROUP "brro"

#define NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY 2
#define NR_BACKHAUL_INTERFACE_PHY_DOWN  3

static mac_api_t *api;
static eth_mac_api_t *eth_mac_api;

/* The border router tasklet runs in grounded/non-storing mode */
#define RPL_FLAGS RPL_GROUNDED | BR_DODAG_MOP_NON_STORING | RPL_DODAG_PREF(0)

typedef enum interface_bootstrap_state {
    INTERFACE_IDLE_PHY_NOT_READY,
    INTERFACE_IDLE_STATE,
    INTERFACE_BOOTSTRAP_ACTIVE,
    INTERFACE_CONNECTED
} interface_bootstrap_state_e;

typedef struct {
    uint8_t DODAG_ID[16];
    uint8_t rpl_instance_id;
    uint8_t rpl_setups;
} rpl_setup_info_t;

typedef struct {
    int8_t prefix_len;
    uint8_t prefix[16];
    uint8_t next_hop[16];
} route_info_t;

/* Border router channel list */
static channel_list_s channel_list;

/* Channel masks for different RF types */
static const uint32_t channel_mask_0_2_4ghz = 0x07fff800;

/* Border router settings */
static border_router_setup_s br;

/* RPL routing settings */
static rpl_setup_info_t rpl_setup_info;

/* DODAG configuration */
static dodag_config_t dodag_config;

/* Backhaul prefix */
static uint8_t backhaul_prefix[16] = {0};

/* Backhaul default route information */
static route_info_t backhaul_route;

/* Should prefix on the backhaul used for PAN as well? */
static uint8_t rf_prefix_from_backhaul = 0;

static net_6lowpan_mode_e operating_mode = NET_6LOWPAN_BORDER_ROUTER;
static net_6lowpan_mode_extension_e operating_mode_extension = NET_6LOWPAN_ND_WITH_MLE;
static interface_bootstrap_state_e net_6lowpan_state = INTERFACE_IDLE_PHY_NOT_READY;
static interface_bootstrap_state_e net_backhaul_state = INTERFACE_IDLE_PHY_NOT_READY;
static net_ipv6_mode_e backhaul_bootstrap_mode = NET_IPV6_BOOTSTRAP_STATIC;

static const uint8_t gp16_address_suffix[6] = {0x00, 0x00, 0x00, 0xff, 0xfe, 0x00};

static int8_t br_tasklet_id = -1;
static int8_t net_6lowpan_id = -1;
static int8_t backhaul_if_id = -1;

/* Network statistics */
static nwk_stats_t nwk_stats;

/* Link layer security information */
static net_6lowpan_link_layer_sec_mode_e link_security_mode;
static net_link_layer_psk_security_info_s link_layer_psk;
static net_tls_cipher_e pana_security_suite;

static uint8_t multicast_addr[16] = {0};

/* Function forward declarations */
static void app_parse_network_event(arm_event_s *event);
static void borderrouter_tasklet(arm_event_s *event);
static void initialize_channel_list(uint32_t channel);
static void start_6lowpan(const uint8_t *backhaul_address);
static void load_config(void);

void border_router_start(void)
{
    platform_timer_enable();
    eventOS_scheduler_init();

    load_config();
    net_init_core();

    protocol_stats_start(&nwk_stats);

    eventOS_event_handler_create(
        &borderrouter_tasklet,
        ARM_LIB_TASKLET_INIT_EVENT);
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
            tr_info("   [%d] %s", i, buf);
            t_buf += 16;
        }
    }
}

static void initialize_channel_list(uint32_t channel)
{
    const int_fast8_t word_index = channel / 32;
    const int_fast8_t bit_index = channel % 32;

    rf_trx_part_e type = rf_radio_type_read();

    switch (type) {
        case ATMEL_AT86RF212:
            tr_debug("Using SUBGHZ radio, type = %d, channel = %lu", type, channel);
            break;
        case ATMEL_AT86RF231:
        case ATMEL_AT86RF233:
            tr_debug("Using 24GHZ radio, type = %d, channel = %lu", type, channel);
            break;
        default:
            tr_debug("Using UNKNOWN radio, type = %d, channel = %lu", type, channel);
            break;
    }

    if (channel > 0) {
        /* Zero channel value means listen all channels */
        memset(&channel_list.channel_mask, 0, sizeof(channel_list.channel_mask));
        channel_list.channel_mask[word_index] |= ((uint32_t) 1 << bit_index);
    }
}

static void load_config(void)
{
    const char *prefix, *psk;
    uint8_t nd_prefix[16];

    prefix = cfg_string(global_config, "PREFIX", NULL);

    if (!prefix) {
        tr_error("No RF prefix in configuration!");
        return;
    }

    stoip6(prefix, strlen(prefix), nd_prefix);

    prefix = cfg_string(global_config, "BACKHAUL_PREFIX", NULL);
    if (!prefix) {
        tr_error("No backhaul prefix in configuration!");
        return;
    }

    stoip6(prefix, strlen(prefix), backhaul_prefix);
    memset(&backhaul_prefix[8], 0, 8);

    prefix = cfg_string(global_config, "MULTICAST_ADDR", NULL);
    if (!prefix) {
        tr_error("No multicast address in configuration!");
        return;
    }

    stoip6(prefix, strlen(prefix), multicast_addr);

    /* Set up channel page and channgel mask */
    memset(&channel_list, 0, sizeof(channel_list));
    channel_list.channel_page = (channel_page_e)cfg_int(global_config, "RF_CHANNEL_PAGE", CHANNEL_PAGE_0);
    channel_list.channel_mask[0] = cfg_int(global_config, "RF_CHANNEL_MASK", channel_mask_0_2_4ghz);

    prefix = cfg_string(global_config, "NETWORK_ID", "NETWORK000000000");
    memcpy(br.network_id, prefix, 16);

    br.mac_panid = cfg_int(global_config, "PAN_ID", 0x0691);
    br.mac_short_adr = cfg_int(global_config, "SHORT_MAC_ADDRESS", 0xffff);
    br.ra_life_time = cfg_int(global_config, "RA_ROUTER_LIFETIME", 1024);
    br.beacon_protocol_id = cfg_int(global_config, "BEACON_PROTOCOL_ID", 4);

    memcpy(br.lowpan_nd_prefix, nd_prefix, 8);
    br.abro_version_num = 0;

    /* RPL routing setup */
    rpl_setup_info.rpl_instance_id = cfg_int(global_config, "RPL_INSTANCE_ID", 1);
    rpl_setup_info.rpl_setups = RPL_FLAGS;

    /* generate DODAG ID */
    memcpy(rpl_setup_info.DODAG_ID, nd_prefix, 8);
    memcpy(&rpl_setup_info.DODAG_ID[8], gp16_address_suffix, 6);
    rpl_setup_info.DODAG_ID[14] = (br.mac_short_adr >> 8);
    rpl_setup_info.DODAG_ID[15] = br.mac_short_adr;

    /* DODAG configuration */
    dodag_config.DAG_DIO_INT_DOUB = cfg_int(global_config, "RPL_IDOUBLINGS", 12);
    dodag_config.DAG_DIO_INT_MIN = cfg_int(global_config, "RPL_IMIN", 9);
    dodag_config.DAG_DIO_REDU = cfg_int(global_config, "RPL_K", 10);
    dodag_config.DAG_MAX_RANK_INC = cfg_int(global_config, "RPL_MAX_RANK_INC", 2048);
    dodag_config.DAG_MIN_HOP_RANK_INC = cfg_int(global_config, "RPL_MIN_HOP_RANK_INC", 128);
    dodag_config.LIFE_IN_SECONDS = cfg_int(global_config, "RPL_LIFETIME_UNIT", 64);
    dodag_config.LIFETIME_UNIT = cfg_int(global_config, "RPL_DEFAULT_LIFETIME", 60);
    dodag_config.DAG_SEC_PCS = cfg_int(global_config, "RPL_PCS", 1);
    dodag_config.DAG_OCP = cfg_int(global_config, "RPL_OCP", 1);

    /* Bootstrap mode for the backhaul interface */
    backhaul_bootstrap_mode = (net_ipv6_mode_e)cfg_int(global_config,
                              "BACKHAUL_BOOTSTRAP_MODE", NET_IPV6_BOOTSTRAP_STATIC);

    rf_prefix_from_backhaul = cfg_int(global_config, "PREFIX_FROM_BACKHAUL", 0);

    /* Backhaul default route */
    memset(&backhaul_route, 0, sizeof(backhaul_route));
    psk = cfg_string(global_config, "BACKHAUL_NEXT_HOP", NULL);

    if (psk) {
        stoip6(psk, strlen(psk), backhaul_route.next_hop);
    }

    psk = cfg_string(global_config, "BACKHAUL_DEFAULT_ROUTE", NULL);

    if (psk) {
        char *prefix, route_buf[255] = {0};

        /* copy the config value to a non-const buffer */
        strncpy(route_buf, psk, sizeof(route_buf) - 1);
        prefix = strtok(route_buf, "/");
        backhaul_route.prefix_len = atoi(strtok(NULL, "/"));

        stoip6(prefix, strlen(prefix), backhaul_route.prefix);
    }

    prefix = cfg_string(global_config, "SECURITY_MODE", "NONE");

    if (strcmp(prefix, "NONE") == 0) {
        link_security_mode = NET_SEC_MODE_NO_LINK_SECURITY;
        tr_warn("Security NOT enabled");
        return;
    }

    psk = cfg_string(global_config, "PSK_KEY", NULL);

    if (!psk) {
        tr_error("No PSK set in configuration!");
        return;
    }

    link_layer_psk.key_id = cfg_int(global_config, "PSK_KEY_ID", 1);
    memcpy(link_layer_psk.security_key, psk, 16);

    if (strcmp(prefix, "PSK") == 0) {
        tr_debug("Using PSK security mode, key ID = %d", link_layer_psk.key_id);
        link_security_mode = NET_SEC_MODE_PSK_LINK_SECURITY;
    } else if (strcmp(prefix, "PANA") == 0) {
        const char *mode = cfg_string(global_config, "PANA_MODE", "PSK");
        link_security_mode = NET_SEC_MODE_PANA_LINK_SECURITY;
        if (strcmp(mode, "ECC") == 0) {
            pana_security_suite = NET_TLS_ECC_CIPHER;
        } else if (strcmp(mode, "ECC+PSK") == 0) {
            pana_security_suite = NET_TLS_PSK_AND_ECC_CIPHER;
        } else {
            pana_security_suite = NET_TLS_PSK_CIPHER;
        }
    }
}

static int8_t rf_interface_init(void)
{
    int8_t rfid = -1;
    int8_t rf_phy_device_register_id = rf_device_register();
    tr_debug("RF device ID: %d", rf_phy_device_register_id);

    if (rf_phy_device_register_id >= 0) {
        mac_description_storage_size_t storage_sizes;
        storage_sizes.device_decription_table_size = 32;
        storage_sizes.key_description_table_size = 3;
        storage_sizes.key_lookup_size = 1;
        storage_sizes.key_usage_size = 3;
        if (!api) {
            api = ns_sw_mac_create(rf_phy_device_register_id, &storage_sizes);
        }
        rfid = arm_nwk_interface_lowpan_init(api, "mesh0");
        tr_debug("RF interface ID: %d", rfid);
    }

    return rfid;
}

static void borderrouter_backhaul_phy_status_cb(uint8_t link_up, int8_t driver_id)
{
    arm_event_s event;
    event.sender = br_tasklet_id;
    event.receiver = br_tasklet_id;

    if (link_up) {
        event.event_id = NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY;
    } else {
        event.event_id = NR_BACKHAUL_INTERFACE_PHY_DOWN;
    }

    tr_debug("Backhaul driver ID: %d", driver_id);

    event.priority = ARM_LIB_MED_PRIORITY_EVENT;
    event.event_type = APPLICATION_EVENT;
    event.event_data = driver_id;
    event.data_ptr = NULL;
    eventOS_event_send(&event);
}

static int backhaul_interface_up(int8_t driver_id)
{
    int retval = -1;
    if (backhaul_if_id != -1) {
        tr_debug("Border RouterInterface already at active state\n");
    } else {
        if (!eth_mac_api) {
            eth_mac_api = ethernet_mac_create(driver_id);
        }

        backhaul_if_id = arm_nwk_interface_ethernet_init(eth_mac_api, "bh0");

        if (backhaul_if_id >= 0) {
            tr_debug("Backhaul interface ID: %d", backhaul_if_id);
            if (memcmp(backhaul_prefix, (const uint8_t[8] ) { 0 }, 8) == 0) {
                memcpy(backhaul_prefix, rpl_setup_info.DODAG_ID, 8);
            }
            arm_nwk_interface_configure_ipv6_bootstrap_set(
                    backhaul_if_id, backhaul_bootstrap_mode, backhaul_prefix);
            arm_nwk_interface_up(backhaul_if_id);
            retval = 0;
        }
    }
    return retval;
}

static int backhaul_interface_down(void)
{
    int retval = -1;
    if (backhaul_if_id != -1) {
        arm_nwk_interface_down(backhaul_if_id);
        backhaul_if_id = -1;
        retval = 0;
    }
    return retval;
}

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
            app_parse_network_event(event);
            break;

        case APPLICATION_EVENT:
            if (event->event_id == NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY) {
                int8_t net_backhaul_id = (int8_t) event->event_data;
                if (net_backhaul_state == INTERFACE_IDLE_PHY_NOT_READY) {
                    net_backhaul_state = INTERFACE_IDLE_STATE;
                }

                if (backhaul_interface_up(net_backhaul_id) != 0) {
                    tr_debug("Backhaul bootstrap start failed");
                } else {
                    tr_debug("Backhaul bootstrap started");
                    net_backhaul_state = INTERFACE_BOOTSTRAP_ACTIVE;
                }
            } else if (event->event_id == NR_BACKHAUL_INTERFACE_PHY_DOWN) {
                if (backhaul_interface_down() != 0) {
                    tr_error("Backhaul interface down failed");
                } else {
                    tr_debug("Backhaul interface is down");
                    backhaul_if_id = -1;
                    net_backhaul_state = INTERFACE_IDLE_STATE;
                }
            }
            break;

        case ARM_LIB_TASKLET_INIT_EVENT:
            br_tasklet_id = event->receiver;

            /* initialize the backhaul interface */
            backhaul_driver_init(borderrouter_backhaul_phy_status_cb);
            /* initialize Radio module*/
            net_6lowpan_id = rf_interface_init();

            if (net_6lowpan_id < 0) {
                tr_error("RF interface initialization failed");
                return;
            }
            net_6lowpan_state = INTERFACE_IDLE_STATE;
            eventOS_event_timer_request(9, ARM_LIB_SYSTEM_TIMER_EVENT, br_tasklet_id, 20000);
            break;

        case ARM_LIB_SYSTEM_TIMER_EVENT:
            eventOS_event_timer_cancel(event->event_id, event->receiver);

            if (event->event_id == 9) {
#ifdef MBED_CONF_APP_DEBUG_TRACE
#if MBED_CONF_APP_DEBUG_TRACE == 1
                arm_print_routing_table();
                arm_print_neigh_cache();
#endif
#endif
                eventOS_event_timer_request(9, ARM_LIB_SYSTEM_TIMER_EVENT, br_tasklet_id, 20000);
            }
            break;

        default:
            break;
    }
}

static void start_6lowpan(const uint8_t *backhaul_address)
{
    uint8_t p[16] = {0};

    if (arm_net_address_get(backhaul_if_id, ADDR_IPV6_GP, p) == 0) {
        uint32_t lifetime = 0xffffffff; // infinite
        uint8_t prefix_len = 0;
        uint8_t t_flags = 0;
        int8_t retval = -1;

        /* Channel list: listen to a channel (default: all channels) */
        uint32_t channel = cfg_int(global_config, "RF_CHANNEL", 0);
        initialize_channel_list(channel);

        // configure as border router and set the operation mode
        retval = arm_nwk_interface_configure_6lowpan_bootstrap_set(net_6lowpan_id,
                 operating_mode, operating_mode_extension);

        if (retval < 0) {
            tr_error("Configuring 6LoWPAN bootstrap failed, retval = %d", retval);
            return;
        }

        retval = arm_nwk_link_layer_security_mode(net_6lowpan_id, link_security_mode, 5, &link_layer_psk);

        if (retval < 0) {
            tr_error("Failed to set link layer security mode, retval = %d", retval);
            return;
        }

        /* Should we use the backhaul prefix on the PAN as well? */
        if (backhaul_address && rf_prefix_from_backhaul) {
            memcpy(br.lowpan_nd_prefix, p, 8);
            memcpy(rpl_setup_info.DODAG_ID, br.lowpan_nd_prefix, 8);
        }

        retval = arm_nwk_6lowpan_border_router_init(net_6lowpan_id, &br);

        if (retval < 0) {
            tr_error("Initializing 6LoWPAN border router failed, retval = %d", retval);
            return;
        }

        /* configure both /64 and /128 context prefixes */
        retval = arm_nwk_6lowpan_border_router_context_update(net_6lowpan_id, ((1 << 4) | 0x03),
                 128, 0xffff, rpl_setup_info.DODAG_ID);

        if (retval < 0) {
            tr_error("Setting ND context failed, retval = %d", retval);
            return;
        }

        // configure the RPL routing protocol for the 6LoWPAN mesh network
        if (arm_nwk_6lowpan_rpl_dodag_init(net_6lowpan_id, rpl_setup_info.DODAG_ID,
                                           &dodag_config, rpl_setup_info.rpl_instance_id,
                                           rpl_setup_info.rpl_setups) == 0) {
            prefix_len = 64;
            t_flags = RPL_PREFIX_ROUTER_ADDRESS_FLAG;
            /* add "/64" prefix with the full BR address (DODAG ID) */
            arm_nwk_6lowpan_rpl_dodag_prefix_update(net_6lowpan_id, rpl_setup_info.DODAG_ID,
                                                    prefix_len, t_flags, lifetime);

            t_flags = 0;
            prefix_len = 0;
            /* add default route "::/0" */
            arm_nwk_6lowpan_rpl_dodag_route_update(net_6lowpan_id, rpl_setup_info.DODAG_ID,
                                                   prefix_len, t_flags, lifetime);
        }

        if (link_security_mode == NET_SEC_MODE_PANA_LINK_SECURITY) {
            uint8_t *psk = (uint8_t *)cfg_string(global_config, "TLS_PSK_KEY", NULL);

            if (!psk) {
                tr_error("No TLS PSK key set in configuration");
                return;
            }

            if (arm_tls_add_psk_key(psk, cfg_int(global_config, "TLS_PSK_KEY_ID", 0)) != 0) {
                tr_error("No TLS PSK key ID set in configuration");
                return;
            }

            retval = arm_pana_server_library_init(net_6lowpan_id, pana_security_suite, NULL, 120);

            if (retval) {
                tr_error("Failed to initialize PANA server library, retval = %d", retval);
                return;
            }
        }

        retval = arm_nwk_set_channel_list(net_6lowpan_id, &channel_list);

        if (retval) {
            tr_error("Failed to set channel list, retval = %d", retval);
            return;
        }

        retval = arm_nwk_interface_up(net_6lowpan_id);

        if (retval < 0) {
            tr_error("Failed to bring up the RF interface, retval = %d", retval);
            return;
        }

        /* mark the RF interface active */
        net_6lowpan_state = INTERFACE_BOOTSTRAP_ACTIVE;

        multicast_set_parameters(10, 0, 20, 3, 75);
        multicast_add_address(multicast_addr, 1);
    }
}

/**
  * \brief Network state event handler.
  * \param event show network start response or current network state.
  *
  */
static void app_parse_network_event(arm_event_s *event)
{
    switch ((arm_nwk_interface_status_type_e)event->event_data) {
        case ARM_NWK_BOOTSTRAP_READY: {
            bool gp_address_available;
            uint8_t p[16];
            char buf[128];
            if (0 == arm_net_address_get(event->event_id, ADDR_IPV6_GP, p)) {
                ip6tos(p, buf);
                gp_address_available = true;
            } else {
                gp_address_available = false;
            }

            if (backhaul_if_id == event->event_id) {

                if (gp_address_available) {
                    tr_info("Backhaul bootstrap ready, IPv6 = %s", buf);
                } else {
                    tr_info("Backhaul interface in ULA Mode");
                }

                if (backhaul_bootstrap_mode == NET_IPV6_BOOTSTRAP_STATIC) {
                    int8_t retval;
                    uint8_t *next_hop_ptr;

                    if (memcmp(backhaul_route.next_hop, (const uint8_t[16]) {0}, 16) == 0) {
                        next_hop_ptr = NULL;
                    }
                    else {
                        next_hop_ptr = backhaul_route.next_hop;
                    }

                    tr_info("Backhaul default route:");
                    tr_info("   prefix:   %s", print_ipv6_prefix(backhaul_route.prefix, backhaul_route.prefix_len));
                    tr_info("   next hop: %s", next_hop_ptr ? print_ipv6(backhaul_route.next_hop) : "on-link");

                    retval = arm_net_route_add(backhaul_route.prefix, backhaul_route.prefix_len,
                                               next_hop_ptr, 0xffffffff, 128, backhaul_if_id);

                    if (retval < 0) {
                        tr_error("Failed to add backhaul default route, retval = %d", retval);
                    }
                }

                tr_info("Backhaul interface addresses:");
                print_interface_addr(backhaul_if_id);

                net_backhaul_state = INTERFACE_CONNECTED;
                if (net_6lowpan_state == INTERFACE_IDLE_STATE) {
                    //Start 6lowpan
                    start_6lowpan(p);
                }
            } else {
                tr_info("RF bootstrap ready, IPv6 = %s", buf);
                arm_nwk_6lowpan_rpl_dodag_start(net_6lowpan_id);
                net_6lowpan_state = INTERFACE_CONNECTED;
                tr_info("RF interface addresses:");
                print_interface_addr(net_6lowpan_id);
                tr_info("6LoWPAN Border Router Bootstrap Complete.");
            }
        }
            /* Network connection Ready */
        break;
        case ARM_NWK_NWK_SCAN_FAIL:
            /* Link Layer Active Scan Fail, Stack is Already in Idle state */
            break;
        case ARM_NWK_IP_ADDRESS_ALLOCATION_FAIL:
            /* No ND Router at current Channel Stack is Already at Idle state */
            break;
        case ARM_NWK_NWK_CONNECTION_DOWN:
            /* Connection to Access point is lost wait for Scan Result */
            break;
        case ARM_NWK_NWK_PARENT_POLL_FAIL:
            break;
        case ARM_NWK_AUHTENTICATION_FAIL:
            /* Network authentication fail */
            break;
        case ARM_NWK_DUPLICATE_ADDRESS_DETECTED:
            if (backhaul_if_id == event->event_id) {
                tr_error("Backhaul DAD failed.");
            }
            break;
        default:
            /* Unknow event */
            break;
    }
}
