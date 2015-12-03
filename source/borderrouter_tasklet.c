/*
 * Copyright (c) 2014-2015 ARM Limited. All rights reserved.
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
#include "eventOS_event_timer.h"
#include "multicast_api.h"
#include "whiteboard_api.h"
#include "nanostack-border-router/borderrouter_tasklet.h"
#include "nanostack-border-router/config_def.h"
#include "atmel-rf-driver/driverRFPhy.h"
//Static bootsrap
#define NET_IPV6_BOOTSTRAP_MODE_S
#include "nanostack-border-router/static_config.h"
#include "ip6string.h"

#define HAVE_DEBUG 1
#include "ns_trace.h"
#define TRACE_GROUP  "brro"

/**
 * /enum address_type_t
 * /brief Address types
 */
typedef enum interface_bootstarp_state {
    INTERFACE_IDLE_PHY_NOT_READY,
    INTERFACE_IDLE_STATE,
    INTERFACE_BOOTSTRAP_ACTIVE,
    INTERFACE_CONNECTED
} interface_bootstarp_state_e;


/*Border Router ND setup*/
border_router_setup_s br;
/*RPL setup data base*/
rpl_setup_info_t rpl_setup_info;
/*Border Router static RPL DODAG configuration*/
dodag_config_t dodag_config;
/*Default prefix*/
uint8_t br_def_prefix[16] = {0};
/*Backhaul prefix*/
uint8_t br_ipv6_prefix[16];
const uint8_t *networkid;


#ifdef NET_IPV6_BOOTSTRAP_MODE_S
net_ipv6_mode_e ipv6_bootstrap_mode = NET_IPV6_BOOTSTRAP_STATIC;
#else
net_ipv6_mode_e ipv6_bootstrap_mode = NET_IPV6_BOOTSTRAP_AUTONOMOUS;
#endif

net_6lowpan_mode_e operating_mode = NET_6LOWPAN_BORDER_ROUTER;
net_6lowpan_mode_extension_e operating_mode_extension = NET_6LOWPAN_ND_WITH_MLE;

interface_bootstarp_state_e net_6lowpan_state = INTERFACE_IDLE_PHY_NOT_READY;
interface_bootstarp_state_e net_backhaul_state = INTERFACE_IDLE_PHY_NOT_READY;

static int8_t br_tasklet_id = -1;
static int8_t net_6lowpan_id = -1;
static int8_t backhaul_if_id = -1;

const uint8_t app_multicast_addr[16] = {0xff, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
uint8_t tls_psk_test_key[16] = {1,2,3,4,5,6,7,9,10,11,12,13,14,15,16};

/* Link layer security information */
net_6lowpan_link_layer_sec_mode_e link_security_mode;
net_tls_cipher_e pana_security_suite;
net_link_layer_psk_security_info_s link_layer_psk;

static void app_parse_network_event(arm_event_s *event);

static void start_6lowpan(void);

void borderrouter_backhaul_phy_status_cb(uint8_t link_up, int8_t driver_id)
{
    arm_event_s event;
	event.sender = br_tasklet_id;
	event.receiver = br_tasklet_id;
	if(link_up)
		event.event_id = NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY;
	else
		event.event_id = NR_BACKHAUL_INTERFACE_PHY_DOWN;
	event.event_type = APPLICATION_EVENT;
	event.event_data = driver_id;
	event.data_ptr = NULL;
	eventOS_event_send(&event);
}

static int app_ipv6_interface_up(int8_t driver_id)
{
	int retval = -1;
	if(backhaul_if_id != -1)
	{
		tr_debug("Border RouterInterface already at active state\n");
	}
	else
	{
		backhaul_if_id = arm_nwk_interface_init(NET_INTERFACE_ETHERNET, driver_id, "BackHaulNet");
		if(backhaul_if_id >= 0)
		{
            tr_debug("Backhaul interface id: %d", backhaul_if_id);
            if (memcmp(br_ipv6_prefix, (const uint8_t[8]) { 0 }, 8) == 0)
				memcpy(br_ipv6_prefix, rpl_setup_info.DODAG_ID, 8);
			arm_nwk_interface_configure_ipv6_bootstrap_set(backhaul_if_id, ipv6_bootstrap_mode, br_ipv6_prefix);
			arm_nwk_interface_up(backhaul_if_id);
			retval = 0;
		}
	}
	return retval;
}


static int app_ipv6_interface_down(void)
{
	int retval = -1;
	if(backhaul_if_id != -1)
	{
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
void borderrouter_tasklet(arm_event_s *event)
{

    arm_library_event_type_e event_type;
    event_type = (arm_library_event_type_e)event->event_type;

    switch(event_type)
    {
      case ARM_LIB_NWK_INTERFACE_EVENT:
        app_parse_network_event(event);
        break;

      case APPLICATION_EVENT:
		if(event->event_id == NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY)
		{
            int8_t net_backhaul_id = (int8_t) event->event_data;
			if (net_backhaul_state == INTERFACE_IDLE_PHY_NOT_READY) {
			    net_backhaul_state = INTERFACE_IDLE_STATE;
			}

			if(app_ipv6_interface_up(net_backhaul_id) != 0)
			{
				tr_debug("IPv6 Interface bootsrap start fail");
			}
			else
			{
				tr_debug("Ipv6 bootsrap started");
				net_backhaul_state = INTERFACE_BOOTSTRAP_ACTIVE;
			}
		}
		else if(event->event_id == NR_BACKHAUL_INTERFACE_PHY_DOWN)
		{
			if(app_ipv6_interface_down() != 0)
			{
				tr_debug("Ipv6 backhaul interface set Down fail\n");
			}
			else
			{
				tr_debug("Ipv6 backhaul interface Down\n");
				backhaul_if_id = -1;
				net_backhaul_state = INTERFACE_IDLE_STATE;
			}
		}
		break;

      case ARM_LIB_TASKLET_INIT_EVENT:
        br_tasklet_id = event->receiver;
    	/* Init Backhaul network */
    	main_borderrouter_driver_init(borderrouter_backhaul_phy_status_cb);

    	net_6lowpan_id = main_6lowpan_interface_init();
        if(net_6lowpan_id < 0)
        {
        	tr_debug("lowpan Interface init Fail\n");
        	return;
        }
        net_6lowpan_state = INTERFACE_IDLE_STATE;
        eventOS_event_timer_request(9,ARM_LIB_SYSTEM_TIMER_EVENT,br_tasklet_id,20000);
        break;

      case ARM_LIB_SYSTEM_TIMER_EVENT:
    	 eventOS_event_timer_cancel(event->event_id, event->receiver);

    	 if (event->event_id == 9) {
             arm_print_routing_table();
    	     arm_print_neigh_cache();
    	     eventOS_event_timer_request(9,ARM_LIB_SYSTEM_TIMER_EVENT,br_tasklet_id,20000);
    	  }
      break;

      default:
      break;
    }
}

void start_6lowpan(void)
{
    uint8_t p[16];
    if (0 == arm_net_address_get(backhaul_if_id, ADDR_IPV6_GP, p)) {

        uint32_t lifetime = 0xffffffff;
        uint8_t t_flags = 0;
        uint8_t prefix_len =0;

        //RF Channel
        if (rf_radio_type_read() == ATMEL_AT86RF212)
        {
            br.channel = cfg_int(global_config, "BR_SUBGHZ_CHANNEL", 1);
        }
        else if (rf_radio_type_read() == ATMEL_AT86RF231)
        {
            br.channel = cfg_int(global_config, "BR_2_4_GHZ_CHANNEL", 12);
        }
        else
        {
            br.channel = cfg_int(global_config, "BR_CHANNEL", 12);
        }

        arm_nwk_interface_configure_6lowpan_bootstrap_set(net_6lowpan_id, operating_mode, operating_mode_extension);

        uint8_t t_prefix[16];
        memset(&br_def_prefix[8], 0, 8);
        // 6LowPAN ND Prefix
        memcpy(br.lowpan_nd_prefix, br_def_prefix, 8);
        // RPL setup
        // DODAG ID
        memcpy(rpl_setup_info.DODAG_ID, br_def_prefix, 8);
        arm_nwk_link_layer_security_mode(net_6lowpan_id, link_security_mode, 5, &link_layer_psk);

        arm_tls_add_psk_key(tls_psk_test_key, 1);

        arm_nwk_6lowpan_border_router_init(net_6lowpan_id,&br);

        if(arm_nwk_6lowpan_rpl_dodag_init(net_6lowpan_id,rpl_setup_info.DODAG_ID,&dodag_config, rpl_setup_info.rpl_instance_id, rpl_setup_info.rpl_setups) == 0)
        {
            memcpy(t_prefix,rpl_setup_info.DODAG_ID, 16 );
            prefix_len = 64;
            t_flags = RPL_PREFIX_ROUTER_ADDRESS_FLAG; //Router Address
            if(memcmp(t_prefix,br.lowpan_nd_prefix,8) != 0)
            {
                t_flags |= RPL_PREFIX_AUTONOMOUS_ADDRESS_FLAG;
            }
            arm_nwk_6lowpan_rpl_dodag_prefix_update(net_6lowpan_id,t_prefix,prefix_len,t_flags,lifetime);

            t_flags = 0x18;

            arm_nwk_6lowpan_rpl_dodag_route_update(net_6lowpan_id,t_prefix,prefix_len,t_flags,lifetime);
            if(memcmp(t_prefix,br.lowpan_nd_prefix,8) != 0)
            {
                memcpy(t_prefix,br.lowpan_nd_prefix,8);
                arm_nwk_6lowpan_rpl_dodag_route_update(net_6lowpan_id,t_prefix,prefix_len,t_flags,lifetime);
                t_flags = 0;
                arm_nwk_6lowpan_rpl_dodag_prefix_update(net_6lowpan_id,t_prefix,prefix_len,t_flags,lifetime);

            }
            t_flags = 0;
            prefix_len = 0;
            arm_nwk_6lowpan_rpl_dodag_route_update(net_6lowpan_id,t_prefix,prefix_len,t_flags,lifetime);
        }
        if(NET_SEC_MODE_PANA_LINK_SECURITY == link_security_mode)
        {
            uint8_t *psk;
            psk = (uint8_t *)cfg_string(global_config, "TLS_PSK_KEY", NULL);

            if(arm_tls_add_psk_key(psk, cfg_int(global_config, "TLS_PSK_KEY_ID", 0)) != 0)
            {
                tr_debug("Failed to get TLS_PSK_KEY from config");
                return;
            }

            if(arm_pana_server_library_init(net_6lowpan_id, pana_security_suite, NULL, 120))
            {
                /* Error starting PANA server */
                return;
            }
        }

        if (arm_nwk_interface_up(net_6lowpan_id) != 0) {
            return;
        } else {
            net_6lowpan_state = INTERFACE_BOOTSTRAP_ACTIVE;
        }

        multicast_set_parameters(10,0,20,3,75 );
        multicast_add_address((uint8_t*)app_multicast_addr, 1);
    }
}

/**
  * \brief Network state event handler.
  * \param event show network start response or current network state.
  *
  */
void app_parse_network_event(arm_event_s *event)
{
    switch ((arm_nwk_interface_status_type_e)event->event_data)
    {
        case ARM_NWK_BOOTSTRAP_READY:
	    {
            bool gp_address_available;
			uint8_t p[16];
			char buf[128];
			if (0 == arm_net_address_get(event->event_id, ADDR_IPV6_GP, p))
			{
			    ip6tos(p, buf);
				gp_address_available = true;
			} else {
			    gp_address_available = false;
			}

			if (backhaul_if_id == event->event_id) {

    			if (gp_address_available) {
    			    tr_debug("Backhaul interface bootstrap ready, IP=%s", buf);
    			} else {
    				tr_debug("\nBackhaul interface bootstrap in Ula Mode");
    			}

    			net_backhaul_state = INTERFACE_CONNECTED;
    			if (net_6lowpan_state == INTERFACE_IDLE_STATE) {
    				//Start 6lowpan
    			    start_6lowpan();
    			}
			}
			else
			{
			    tr_debug("6Lowpan bootstrap ready, IP=%s", buf);
				arm_nwk_6lowpan_rpl_dodag_start(net_6lowpan_id);
				net_6lowpan_state = INTERFACE_CONNECTED;
			}
		}
        /* Network connection Ready */
        break;
        case ARM_NWK_NWK_SCAN_FAIL:
        /* Link Layer Active Scan Fail, Stack is Already at Idle state */
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
		if (backhaul_if_id == event->event_id)
		{
			tr_debug("Backhaul interface IPv6 DAD detected!!!!\n");
		}
		break;
        default:
        /* Unknow event */
        break;
   }
}
