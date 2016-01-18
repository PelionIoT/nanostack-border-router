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

#include "mbed-drivers/mbed.h"
#include "eventOS_event.h"
#include "eventOS_scheduler.h"
#include "platform/arm_hal_phy.h"
#include "atmel-rf-driver/driverRFPhy.h"
#include "nwk_stats_api.h"
#include "nsdynmemLIB.h"
#include "randLIB.h"
#include "net_interface.h"
#include "platform/arm_hal_timer.h"
#include "nanostack-border-router/borderrouter_tasklet.h"
#include "sal-stack-nanostack-slip/Slip.h"
#include "nanostack-border-router/cfg_parser.h"
#include "static_config.h"
#include "ip6string.h"
#define HAVE_DEBUG 1
#include "ns_trace.h"
#define TRACE_GROUP  "brTEST"
#include "mbed-drivers/app.h"

#define APP_DEFINED_HEAP_SIZE 32500

static Serial pc(USBTX, USBRX);

static SlipMACDriver *pslipmacdriver;

static uint8_t app_stack_heap[APP_DEFINED_HEAP_SIZE +1];

/*Network statistics Buffer*/
static nwk_stats_t nwk_stats;
/*Default security key (if used)*/
const uint8_t *net_security_key;

static void app_heap_error_handler(heap_fail_t event);

//Blink LED
static DigitalOut led1(LED1);

static void toggleLED1()
{
    led1 = !led1;
}

static void stop(void)
{
	while(1)
		;;
}

void trace_printer(const char* str)
{
    pc.printf("%s\r\n", str);
}

static void load_config(void)
{
	const char *prefix;
	const char *backhaul_prefix;
	const char *security_mode;
	const char *pana_mode;
	const char *psk;

	prefix = cfg_string(global_config, "PREFIX", NULL);
	if (prefix) {
		stoip6(prefix, strlen(prefix), br_def_prefix);
		backhaul_prefix = cfg_string(global_config, "BACKHAUL_PREFIX", NULL);
		if (!backhaul_prefix)
			backhaul_prefix = prefix;
		// IPV6 Prefix
		stoip6(backhaul_prefix, strlen(backhaul_prefix), br_ipv6_prefix);
	}

	networkid = (const uint8_t*)cfg_string(global_config, "NETWORKID", NULL);
	if (!networkid) {
		debug("Failed to get NETWORKID from config\n");
		stop();
	}

	net_security_key = (const uint8_t*)cfg_string(global_config, "PSK_KEY", NULL);

  	// Network ID
  	memcpy(br.network_id, cfg_string(global_config, "NETWORKID", "NETWORK000000001"), 16);
  	// PAN ID
  	br.mac_panid = cfg_int(global_config, "BR_PAN_ID", 0x0691);
  	//16-bit Short Address
  	br.mac_short_adr = cfg_int(global_config, "BR_SHORT_ADDRESS", randLIB_get_16bit());
  	// 6LowPAN ND Prefix
  	memcpy(br.lowpan_nd_prefix, br_def_prefix, 8);
  	// RPL setup
  	rpl_setup_info.rpl_instance_id = cfg_int(global_config, "BR_RPL_INSTANCE_ID", 1);;
  	rpl_setup_info.rpl_setups = cfg_int(global_config, "BR_RPL_FLAGS", 0);
  	//ND Lifetime
  	br.ra_life_time = cfg_int(global_config, "BR_ND_ROUTE_LIFETIME", 1024);
  	// Beacon protocol ID
  	br.beacon_protocol_id = cfg_int(global_config, "BR_BEACON_PROTOCOL_ID", 4);
  	// DODAG configuration
  	dodag_config.DAG_DIO_INT_DOUB = cfg_int(global_config, "BR_DAG_DIO_INT_DOUB", 12);
  	dodag_config.DAG_DIO_INT_MIN = cfg_int(global_config, "BR_DAG_DIO_INT_MIN", 9);
  	dodag_config.DAG_DIO_REDU = cfg_int(global_config, "BR_DAG_DIO_REDU", 10);
  	dodag_config.DAG_MAX_RANK_INC = cfg_int(global_config, "BR_DAG_MAX_RANK_INC", 16);
  	/* Backwards compatibility fudge - old name */
  	dodag_config.DAG_MIN_HOP_RANK_INC = cfg_int(global_config, "BR_DAG_MIN_RANK_INC", 128);
  	dodag_config.DAG_MIN_HOP_RANK_INC = cfg_int(global_config, "BR_DAG_MIN_HOP_RANK_INC", dodag_config.DAG_MIN_HOP_RANK_INC);
  	dodag_config.LIFE_IN_SECONDS = cfg_int(global_config, "BR_LIFE_IN_SECONDS", 64);
  	dodag_config.LIFETIME_UNIT = cfg_int(global_config, "BR_LIFETIME_UNIT", 60);
  	dodag_config.DAG_SEC_PCS = cfg_int(global_config, "BR_DAG_SEC_PCS", 1);
  	dodag_config.DAG_OCP = cfg_int(global_config, "BR_DAG_OCP", 1);

  	// DODAG ID
  	prefix = cfg_string(global_config, "RPL_PREFIX", NULL);
  	if (!prefix) {
		memcpy(rpl_setup_info.DODAG_ID, br_def_prefix, 8);
  	}
  	else
	  stoip6(prefix, strlen(prefix), rpl_setup_info.DODAG_ID);

  	prefix = cfg_string(global_config, "APP_SHORT_ADDRESS_SUFFIX", NULL);
  	if (!prefix) {
    	debug("Failed to get short address suffix from config\n");
    	stop();
  	}
  	memcpy(&rpl_setup_info.DODAG_ID[8], prefix, 6);
  	rpl_setup_info.DODAG_ID[14] = (br.mac_short_adr >> 8);
  	rpl_setup_info.DODAG_ID[15] = br.mac_short_adr;

	//Security setups
	security_mode = cfg_string(global_config, "SECURITY_MODE", "NONE");
	if(!strcmp(security_mode, "PANA"))
	{
		link_security_mode = NET_SEC_MODE_PANA_LINK_SECURITY;

		pana_mode = cfg_string(global_config, "PANA_MODE", "PSK");
		if(!strcmp(pana_mode, "ECC"))
		{
			pana_security_suite = NET_TLS_ECC_CIPHER;
		}
		else if(!strcmp(pana_mode, "ECC+PSK"))
		{
			pana_security_suite = NET_TLS_PSK_AND_ECC_CIPHER;
		}
		else
		{
			pana_security_suite = NET_TLS_PSK_CIPHER;
		}
		psk = cfg_string(global_config, "PSK_KEY", NULL);

		if(!psk)
		{
			debug("Failed to get PSK_KEY from config\n");
			//stop();
		}
		else
			memcpy(link_layer_psk.security_key, psk, 16);
	}
	else if(!strcmp(security_mode, "PSK"))
	{
		link_security_mode = NET_SEC_MODE_PSK_LINK_SECURITY;

		psk = cfg_string(global_config, "PSK_KEY", NULL);

		if(!psk)
		{
			debug("Failed to get PSK_KEY from config\n");
			stop();
		}

		memcpy(link_layer_psk.security_key, psk, 16);
		link_layer_psk.key_id = 1;

	}
	else
	{
		link_security_mode = NET_SEC_MODE_NO_LINK_SECURITY;
	}

}

/**
 * \brief This function will be called border router tasklet handle init event.
 */
void main_borderrouter_driver_init(void (*backhaul_driver_status_cb)(uint8_t,int8_t))
{
	int8_t slipDrv_id;
	slipDrv_id = pslipmacdriver->Slip_Init();
	tr_debug("Slip phy id: %d", slipDrv_id);
	if(slipDrv_id >= 0)
	{
		backhaul_driver_status_cb(1, slipDrv_id);
	}
}

int8_t main_6lowpan_interface_init(void)
{
	int8_t lowpanId = -1;
	char iName[] = "6LoWPAN_BORDER_ROUTER";

	int8_t rf_phy_device_register_id = rf_device_register();
	tr_debug("rf phy id: %d", rf_phy_device_register_id);

	if(rf_phy_device_register_id >= 0)
	{
		lowpanId = arm_nwk_interface_init(NET_INTERFACE_RF_6LOWPAN, rf_phy_device_register_id, iName);
		tr_debug("rf interface id: %d", lowpanId);
	}
	return lowpanId;
}

void app_start(int, char **)
{
    pc.baud(115200);  //Setting the Baud-Rate for trace output

	ns_dyn_mem_init(app_stack_heap, APP_DEFINED_HEAP_SIZE, app_heap_error_handler,0);

    platform_timer_enable();
    eventOS_scheduler_init();

    trace_init();
    set_trace_print_function( trace_printer );
    set_trace_config(TRACE_MODE_COLOR|TRACE_ACTIVE_LEVEL_DEBUG|TRACE_CARRIAGE_RETURN);

    load_config();
    net_init_core();

    protocol_stats_start(&nwk_stats);

	minar::Scheduler::postCallback(mbed::util::FunctionPointer0<void>(toggleLED1).bind())
	.period(minar::milliseconds(500));

	//UART1
	pslipmacdriver = new SlipMACDriver(PTE0, PTE1);

	if(pslipmacdriver == NULL)
	{
		pc.printf("Unable to create SLIPDriver. Insufficient memory!!!!\n\r");
		return;
	}

	pc.printf("Starting K64F border router!\n\r");
    eventOS_event_handler_create(&borderrouter_tasklet, ARM_LIB_TASKLET_INIT_EVENT);

    return;
}

void app_heap_error_handler(heap_fail_t event)
{
	switch (event)
	{
		case NS_DYN_MEM_NULL_FREE:
			debug("\n");
			break;
		case NS_DYN_MEM_DOUBLE_FREE:
			debug("\n");
			break;

		case NS_DYN_MEM_ALLOCATE_SIZE_NOT_VALID:
			debug("\n");
			break;
		case NS_DYN_MEM_POINTER_NOT_VALID:
			debug("\n");
			break;

		case NS_DYN_MEM_HEAP_SECTOR_CORRUPTED:
			debug("\n");
			break;

		case NS_DYN_MEM_HEAP_SECTOR_UNITIALIZED:
			debug("\n");
			break;
		default:

			break;
	}
	while(1);
}
