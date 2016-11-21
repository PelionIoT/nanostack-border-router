/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */


#ifdef MBED_CONF_APP_THREAD_BR

#include <string.h>
#include <stdlib.h>
#include <mbed_assert.h>
#include "ns_types.h"
#include "eventOS_event.h"
#include "eventOS_event_timer.h"
#include "eventOS_scheduler.h"
#include "platform/arm_hal_timer.h"
#include "nanostack-border-router/borderrouter_tasklet.h"
#include "nanostack-border-router/borderrouter_helpers.h"
#include "rf_wrapper.h"
#include "nwk_stats_api.h"
#include "net_interface.h"
#include "ip6string.h"
#include "mac_api.h"
#include "ethernet_mac_api.h"
#include "sw_mac.h"
#include "thread_management_if.h"
#include "thread_dhcpv6_server.h"
#include "thread_interface_status.h"
#include "nanostack-border-router/mbed_config.h"
#include "randLIB.h"

#include "ns_trace.h"
#define TRACE_GROUP "brro"

#define NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY 2
#define NR_BACKHAUL_INTERFACE_PHY_DOWN  3
#define MESH_LINK_TIMEOUT 100
#define MESH_METRIC 1000
#define THREAD_MAX_CHILD_COUNT 32

static mac_api_t *api;
static eth_mac_api_t *eth_mac_api;

typedef enum interface_bootstrap_state {
    INTERFACE_IDLE_PHY_NOT_READY,
    INTERFACE_IDLE_STATE,
    INTERFACE_BOOTSTRAP_ACTIVE,
    INTERFACE_CONNECTED
} interface_bootstrap_state_e;

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
static int8_t backhaul_if_id = -1;

/* Network statistics */
static nwk_stats_t nwk_stats;

/* Function forward declarations */

static void thread_link_configuration_get(link_configuration_s *link_configuration);
static void network_interface_event_handler(arm_event_s *event);

static void meshnetwork_up();
static void mesh_network_data_init(int8_t rf_driver_id);
static void set_network_PSKd(uint8_t *pskd, uint8_t len);
static interface_bootstrap_state_e net_6lowpan_state = INTERFACE_IDLE_PHY_NOT_READY;
static interface_bootstrap_state_e net_backhaul_state = INTERFACE_IDLE_PHY_NOT_READY;
static net_ipv6_mode_e backhaul_bootstrap_mode = NET_IPV6_BOOTSTRAP_STATIC;
static void initialize_channel_list(uint32_t channel);
static void borderrouter_tasklet(arm_event_s *event);
static void initialize_channel_list(uint32_t channel);

/* Timers */
#define RETRY_BOOT_TIMER    70

// pois
typedef enum {
    STATE_UNKNOWN,
    STATE_DISCONNECTED,
    STATE_LINK_READY,
    STATE_BOOTSTRAP,
    STATE_CONNECTED,
    STATE_MAX_VALUE
} connection_state_e;


typedef struct {
    char      *PSKd;
    uint8_t   PSKd_len;
    uint8_t   master_key[16];
    uint8_t   PSKc[16];
    uint8_t   security_policy;
    uint8_t   maxChildCount;
    uint32_t  key_sequence_counter;
    uint32_t  key_rotation;
    uint64_t  timestamp;    
    uint8_t   extented_panid[8];
    uint8_t   private_mac[8];
    uint8_t   ml_eid[8];
    char      *provisioning_url;//ks: ?
} thread_config_t;


typedef struct {
    int8_t              driver_id;
    connection_state_e  state;    
    bool                ifup_ongoing;    
    uint8_t             mac48[6];
    uint8_t             global_address[16];
    route_info_t        default_route;
    uint16_t            metric;
} if_ethernet_t;


typedef struct {
    connection_state_e state;
    int8_t          rf_driver_id;
    uint8_t         network_name[16];
    uint8_t         protocol_id;
    uint8_t         protocol_version;
    int8_t          net_rf_id;
    uint8_t         mesh_ml_prefix[8];    
    bool            ifup_ongoing;
    bool            retry_boot;
    bool            reconnect;
    uint32_t        pan_id;
    uint32_t        rfChannel;
    uint32_t        retry_boot_timeout;
    uint32_t        retry_boot_init;
    uint32_t        retry_boot_max_time;
    uint16_t        retry_boot_max_count;   
    uint32_t         linkTimeout;
    int32_t         num_boot_retries;
    int32_t         num_nwk_reconnections;
    uint16_t        metric;
    thread_config_t thread_cfg;

} if_mesh_t;


typedef struct {
    if_mesh_t       mesh;
    if_ethernet_t   ethernet;        
    int8_t tasklet_id;
} network_configuration_t;

/** Net start operation mode and feature variables*/
network_configuration_t network;

static void set_network_PSKd(uint8_t *PSKd, uint8_t len)
{
    if (network.mesh.thread_cfg.PSKd) {
        free(network.mesh.thread_cfg.PSKd);
    }
    network.mesh.thread_cfg.PSKd_len = len;
    network.mesh.thread_cfg.PSKd = malloc(len + 1);
    memset(network.mesh.thread_cfg.PSKd, 0, len + 1);
    memcpy(network.mesh.thread_cfg.PSKd, PSKd, len);
}

static void mesh_network_data_init(int8_t rf_driver_id)
{
    const char *param;	
    /* Set up channel page and channel mask */
    memset(&channel_list, 0, sizeof(channel_list));
    channel_list.channel_page = MBED_CONF_APP_RF_CHANNEL_PAGE;
    channel_list.channel_mask[0] = MBED_CONF_APP_RF_CHANNEL_MASK;
    
	network.mesh.rfChannel = MBED_CONF_APP_RF_CHANNEL;
	MBED_ASSERT(network.mesh.rfChannel<28);
    initialize_channel_list(network.mesh.rfChannel);
	
    memset(&backhaul_prefix[8], 0, 8);  

    /* Bootstrap mode for the backhaul interface */
    #if MBED_CONF_APP_BACKHAUL_DYNAMIC_BOOTSTRAP == 1
        backhaul_bootstrap_mode = NET_IPV6_BOOTSTRAP_AUTONOMOUS;
        tr_info("NET_IPV6_BOOTSTRAP_AUTONOMOUS");
    
    #else
		tr_info("NET_IPV6_BOOTSTRAP_STATIC");
        backhaul_bootstrap_mode = NET_IPV6_BOOTSTRAP_STATIC;								
		// done like this so that prefix can be left out in the dynamic case.
		param = MBED_CONF_APP_BACKHAUL_PREFIX; 		
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

    //initialize network related variables	
	MBED_ASSERT(strlen(MBED_CONF_APP_NETWORK_NAME)>0 && strlen(MBED_CONF_APP_NETWORK_NAME) < 17);
    param = MBED_CONF_APP_NETWORK_NAME;	
    (void)memcpy((void *)network.mesh.network_name, param, sizeof(param));
    network.mesh.rf_driver_id = rf_driver_id;
    network.mesh.net_rf_id = -1;    
    network.mesh.linkTimeout = MESH_LINK_TIMEOUT;
    network.mesh.metric = MESH_METRIC; // low priority    

    // Thread specific initialization
    network.mesh.thread_cfg.security_policy = 0xff;
    network.mesh.thread_cfg.key_rotation = 3600;
    network.mesh.thread_cfg.key_sequence_counter = 0;
    network.mesh.thread_cfg.provisioning_url = NULL;
    network.mesh.thread_cfg.timestamp = 0;
	const uint8_t pskc[] = MBED_CONF_APP_PSKC;
	MBED_ASSERT(sizeof(pskc)==16);
    (void)memcpy((void *)network.mesh.thread_cfg.PSKc, pskc, sizeof(pskc));
	
	param = MBED_CONF_APP_PSKD;
	uint16_t len = strlen(param);
	MBED_ASSERT(len>5 && len <33);
    set_network_PSKd((uint8_t *)param, len);
	
    const uint8_t master_key[] = MBED_CONF_APP_THREAD_MASTER_KEY;
    MBED_ASSERT(sizeof(master_key) == 16);
    memcpy(network.mesh.thread_cfg.master_key, master_key, sizeof(master_key));    
   
    (void)memcpy((void *)network.mesh.thread_cfg.extented_panid, "\0\0\0\0\0\0\0\0", sizeof(network.mesh.thread_cfg.extented_panid));
    (void)memset((void *)network.mesh.thread_cfg.private_mac, 0, sizeof(network.mesh.thread_cfg.private_mac));
    (void)memset((void *)network.mesh.thread_cfg.ml_eid, 0, sizeof(network.mesh.thread_cfg.ml_eid));
		
    network.mesh.thread_cfg.maxChildCount = THREAD_MAX_CHILD_COUNT;        
    network.mesh.pan_id = MBED_CONF_APP_PAN_ID;
    network.mesh.protocol_id = 0x03;
    network.mesh.protocol_version = 2;    
    memcpy(network.mesh.mesh_ml_prefix, "\xFD\0\x0d\xb8\0\0\0\0", sizeof(network.mesh.mesh_ml_prefix));
    network.ethernet.driver_id = -1;
    network.ethernet.state = STATE_DISCONNECTED;    

    network.ethernet.ifup_ongoing = false;
    network.ethernet.metric = 0; // high priority    
    
	uint8_t rf_mac[6] = {0};
    rf_read_mac_address(rf_mac);
    memcpy(network.ethernet.mac48, rf_mac, sizeof(rf_mac));	

    network.mesh.ifup_ongoing = false;    
    network.mesh.retry_boot = false;
    network.mesh.reconnect = false;
    network.mesh.retry_boot_timeout = 0;
    network.mesh.retry_boot_init = 3000;
    network.mesh.retry_boot_max_count = 0;
    network.mesh.retry_boot_max_time = 9000;
    network.mesh.num_boot_retries = 0;
    network.mesh.num_nwk_reconnections = 0;    
    memset(network.ethernet.global_address, 0, sizeof(network.ethernet.global_address));
}

void retry_boot(void)
{    
    network.mesh.num_boot_retries++;
    tr_debug("Boot scan retry attempt #%d", (int)network.mesh.num_boot_retries);
    if (network.mesh.num_nwk_reconnections > 0) {
        tr_debug("Reconnection attempt #%d", (int)network.mesh.num_nwk_reconnections);
    }
    meshnetwork_up(); 
}

int retry_boot_delay_check(void)
{
	tr_info("retry_boot_delay_check");
    if (network.mesh.retry_boot_max_count > 0 ) {
        if (network.mesh.num_boot_retries >= network.mesh.retry_boot_max_count) {
			tr_warning("max num_boot_retries reached");
            return -1;
        }
    }

    // Check if delay is disabled
    if (network.mesh.retry_boot_init == 0) {
        return 0;
    }

    if (network.mesh.retry_boot_timeout == 0) {
        // First timeout
        network.mesh.retry_boot_timeout = network.mesh.retry_boot_init;
    } else {
        // Next timeouts
        network.mesh.retry_boot_timeout = network.mesh.retry_boot_timeout * 2;
    }

    network.mesh.retry_boot_timeout = network.mesh.retry_boot_timeout * ((rand() % 1000) + 500) / 1000;

    if (network.mesh.retry_boot_max_time > 0) {
        if (network.mesh.retry_boot_timeout > network.mesh.retry_boot_max_time) {
            // maximum for timeout is reached
            network.mesh.retry_boot_timeout = network.mesh.retry_boot_max_time * ((rand() % 1000) + 500) / 1000;
        }
    }

    eventOS_event_timer_request(RETRY_BOOT_TIMER, ARM_LIB_SYSTEM_TIMER_EVENT, network.tasklet_id, network.mesh.retry_boot_timeout);
    return 1;
}


static int thread_interface_up()
{
    int32_t val;    
    device_configuration_s device_config;
    link_configuration_s link_setup;

	tr_info("thread_interface_up");
    memset(&device_config, 0, sizeof(device_config));
    device_config.PSKd_ptr = (uint8_t *)network.mesh.thread_cfg.PSKd;
    device_config.PSKd_len = network.mesh.thread_cfg.PSKd_len;
    device_config.provisioning_uri_ptr = network.mesh.thread_cfg.provisioning_url;

    memset(&link_setup, 0, sizeof(link_setup));    
    thread_link_configuration_get(&link_setup);
    
    val = thread_management_node_init(network.mesh.net_rf_id, &channel_list, &device_config, &link_setup);
	
    if (val) {
        tr_error("Thread init error with code: %is\r\n", (int)val);
		return val;
    }

    // Additional thread configurations    
    thread_management_set_link_timeout(network.mesh.net_rf_id, network.mesh.linkTimeout);   
    thread_management_max_child_count(network.mesh.net_rf_id, network.mesh.thread_cfg.maxChildCount);        

    val = arm_nwk_interface_up(network.mesh.net_rf_id);
    if (val != 0) {
        tr_error("\rmesh0 up Fail with code: %i\r\n", (int)val);
        network.mesh.state = STATE_UNKNOWN;
        network.mesh.ifup_ongoing = false;
        return -1;
    } else {
        tr_info("\rmesh0 bootstrap ongoing..\r\n");
        network.mesh.state = STATE_BOOTSTRAP;
        network.mesh.ifup_ongoing = true;
    }

    return 0;
}

static void thread_link_configuration_get(link_configuration_s *link_configuration)
{
    memset(link_configuration, 0, sizeof(link_configuration_s));    
    memcpy(link_configuration->name, network.mesh.network_name, 16);    
    memcpy(link_configuration->extented_pan_id , network.mesh.thread_cfg.extented_panid, 8);    
    memcpy(link_configuration->extended_random_mac, network.mesh.thread_cfg.private_mac, 8);    
    memcpy(link_configuration->PSKc, network.mesh.thread_cfg.PSKc, 16);    
    memcpy(link_configuration->mesh_local_eid, network.mesh.thread_cfg.ml_eid, 8);    
    link_configuration->timestamp = network.mesh.thread_cfg.timestamp;    
    link_configuration->panId = network.mesh.pan_id;    
    link_configuration->rfChannel = network.mesh.rfChannel;
    link_configuration->securityPolicy = network.mesh.thread_cfg.security_policy;
    //Beacon data setting
    link_configuration->Protocol_id = network.mesh.protocol_id;
    link_configuration->version = network.mesh.protocol_version;
    memcpy(link_configuration->mesh_local_ula_prefix, network.mesh.mesh_ml_prefix, 8); // mesh_local_ula_prefix ei saa näkyä ulkona
    memcpy(link_configuration->master_key, network.mesh.thread_cfg.master_key, 16);
    link_configuration->key_rotation = network.mesh.thread_cfg.key_rotation;
    link_configuration->key_sequence = network.mesh.thread_cfg.key_sequence_counter;
}

static void network_interface_event_handler(arm_event_s *event)
{
    bool connectStatus = false;
    arm_nwk_interface_status_type_e status = (arm_nwk_interface_status_type_e)event->event_data;
    switch (status) {
        case (ARM_NWK_BOOTSTRAP_READY): { // Interface configured Bootstrap is ready

            connectStatus = true;
            tr_info("BR interface_id %d.", thread_interface_status_border_router_interface_id_get());
            if (-1 != thread_interface_status_border_router_interface_id_get()) {
                if (0 == arm_net_address_get(thread_interface_status_border_router_interface_id_get(), ADDR_IPV6_GP, network.ethernet.global_address)) {
                    network.ethernet.state = STATE_CONNECTED;                    
                    tr_info("Ethernet (eth0) bootstrap ready. IP: %s", print_ipv6(network.ethernet.global_address));

                    if (0 != arm_net_interface_set_metric(thread_interface_status_border_router_interface_id_get(), network.ethernet.metric)) {
                        tr_warn("Failed to set metric for eth0.");
                    }

                    if (backhaul_bootstrap_mode==NET_IPV6_BOOTSTRAP_STATIC) {
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
                                          thread_interface_status_border_router_interface_id_get());
                    }

                    thread_interface_status_prefix_add(network.ethernet.global_address, 64);

                } else {
                    tr_warn("arm_net_address_get fail");
                }
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
    //Update Interface status
    if (connectStatus) {        
    } else {        
        network.ethernet.state = STATE_LINK_READY;
    }
    if (network.ethernet.ifup_ongoing) {
        network.ethernet.ifup_ongoing = false;       
    }
    thread_interface_status_ethernet_connection(connectStatus);
}

void thread_interface_event_handler(arm_event_s *event)
{
    bool connectStatus = false;
    arm_nwk_interface_status_type_e status = (arm_nwk_interface_status_type_e)event->event_data;
    switch (status) {
        case (ARM_NWK_BOOTSTRAP_READY): { // Interface configured Bootstrap is ready
            connectStatus = true;
            tr_info("\rBootstrap ready\r\n");

            if (arm_net_interface_set_metric(network.mesh.net_rf_id, network.mesh.metric) != 0) {
                tr_warn("Failed to set metric for mesh0.");
            }
            break;
        }
        case (ARM_NWK_RPL_INSTANCE_FLOODING_READY): // RPL instance have been flooded
            tr_info("\rRPL instance have been flooded\r\n");
            break;
        case (ARM_NWK_SET_DOWN_COMPLETE):
			tr_info("\rThread interface down\r\n");
            break;
        case (ARM_NWK_NWK_SCAN_FAIL):   // Interface have not detect any valid network
            tr_warning("\rmesh0 haven't detect any valid nw\r\n");
            break;
        case (ARM_NWK_IP_ADDRESS_ALLOCATION_FAIL): // IP address allocation fail(ND, DHCPv4 or DHCPv6)
            tr_error("\rNO GP address detected\r\n");
            // Force mesh network re-connection
            if (network.mesh.reconnect) {
                network.mesh.num_nwk_reconnections++;
                network.mesh.ifup_ongoing = true;
            }
            break;
        case (ARM_NWK_DUPLICATE_ADDRESS_DETECTED): // User specific GP16 was not valid
            tr_error("\rEthernet IPv6 Duplicate addr detected!\r\n");            
            break;
        case (ARM_NWK_AUHTENTICATION_START_FAIL): // No valid Authentication server detected behind access point
            tr_error("\rNo valid ath server detected behind AP\r\n");
            break;
        case (ARM_NWK_AUHTENTICATION_FAIL): // Network authentication fail by Handshake
            tr_error("\rNetwork authentication fail\r\n");
            break;
        case (ARM_NWK_NWK_CONNECTION_DOWN): // No connection between Access point or Default Router
            tr_warning("\rPrefix timeout\r\n");
            break;
        case (ARM_NWK_NWK_PARENT_POLL_FAIL): // Sleepy host poll fail 3 time
            tr_error("\rParent poll fail\r\n");
            break;
        default:
            tr_warning("\rUnkown nw if event (type: %02x, id: %02x, data: %02x)\r\n", event->event_type, event->event_id, (unsigned int)event->event_data);
            break;
    }
    if (connectStatus) {
        network.mesh.state = STATE_CONNECTED;
    } else {
        network.mesh.state = STATE_LINK_READY;
    }

    if (network.mesh.ifup_ongoing) {
        if (network.mesh.state != STATE_CONNECTED && (network.mesh.reconnect || network.mesh.retry_boot)) {
            int retry_boot_delay = retry_boot_delay_check();
            if (retry_boot_delay == 0) {
                retry_boot();
            } else if (retry_boot_delay < 0) {
                network.mesh.ifup_ongoing = false;                
            }
        } else {
            network.mesh.ifup_ongoing = false;            
        }
    }
    thread_interface_status_thread_connection(connectStatus);
}

static void meshnetwork_up() {
        tr_info("mesh0 up");

        if (network.mesh.state == STATE_CONNECTED || network.mesh.state == STATE_BOOTSTRAP) {
            tr_info("mesh0 already up\r\n");
        }        

        if (network.mesh.rf_driver_id != -1) {
            thread_interface_status_thread_driver_id_set(network.mesh.rf_driver_id);
            // Create 6Lowpan Interface
            tr_debug("Create Mesh Interface");
            if (network.mesh.net_rf_id == -1) {
                
				network.mesh.net_rf_id = arm_nwk_interface_lowpan_init(api, "ThreadInterface");
				tr_info("network.mesh.net_rf_id: %d", network.mesh.net_rf_id);                    
				thread_interface_status_threadinterface_id_set(network.mesh.net_rf_id);                
                
				arm_nwk_interface_configure_6lowpan_bootstrap_set(
					network.mesh.net_rf_id,
					NET_6LOWPAN_ROUTER,
					NET_6LOWPAN_THREAD);                
            }

            if (network.mesh.net_rf_id != -1) {                
				int err = thread_interface_up();
				if(err) {
					 tr_error("thread_interface_up() failed: %d", err);
				}
            }
        }
    }

void thread_rf_init() {    
    
    int8_t rf_phy_device_register_id = -1;
    rf_phy_device_register_id = rf_device_register();
    mac_description_storage_size_t storage_sizes;
    storage_sizes.key_lookup_size = 1;
    storage_sizes.key_usage_size = 1;
    storage_sizes.device_decription_table_size = 32;
    storage_sizes.key_description_table_size = 6;

    randLIB_seed_random();
    mesh_network_data_init(rf_phy_device_register_id);

    if (!api) {
        api = ns_sw_mac_create(rf_phy_device_register_id, &storage_sizes);
    }
}

static void initialize_channel_list(uint32_t channel)
{
    const int_fast8_t word_index = channel / 32;
    const int_fast8_t bit_index = channel % 32;

    if (channel > 0) {
        /* Zero channel value means listen all channels */
        memset(&channel_list.channel_mask, 0, sizeof(channel_list.channel_mask));
        channel_list.channel_mask[word_index] |= ((uint32_t) 1 << bit_index);
    }
}

void border_router_start(void)
{
    platform_timer_enable();
    eventOS_scheduler_init();    
    net_init_core();
    protocol_stats_start(&nwk_stats);

    eventOS_event_handler_create(
        &borderrouter_tasklet,
        ARM_LIB_TASKLET_INIT_EVENT);
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
    tr_debug("backhaul_interface_up: %i\n", driver_id);
    if (backhaul_if_id != -1) {
        tr_debug("Border RouterInterface already at active state\n");
    } else {

        thread_interface_status_borderrouter_driver_id_set(driver_id);        

        if (!eth_mac_api) {
            eth_mac_api = ethernet_mac_create(driver_id);
        }

        backhaul_if_id = arm_nwk_interface_ethernet_init(eth_mac_api, "bh0");

        if (backhaul_if_id >= 0) {
            tr_debug("Backhaul interface ID: %d", backhaul_if_id);            
            thread_interface_status_borderrouter_interface_id_set(backhaul_if_id);        
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
        // ks:tbd
        thread_interface_status_borderrouter_interface_id_set(-1);
        thread_interface_status_ethernet_connection(false);
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

            if (event->event_id == thread_interface_status_border_router_interface_id_get()) {
               network_interface_event_handler(event);
            } else {
               thread_interface_event_handler(event);
            }            

            break;
        // comes from the backhaul_driver_init.
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
                    // may happend when booting first time.
                    tr_warning("Backhaul interface down failed");
                } else {
                    tr_debug("Backhaul interface is down");
                    backhaul_if_id = -1;
                    net_backhaul_state = INTERFACE_IDLE_STATE;
                }
            }
            break;

        case ARM_LIB_TASKLET_INIT_EVENT:
            br_tasklet_id = event->receiver;            
            backhaul_driver_init(borderrouter_backhaul_phy_status_cb);
            thread_interface_status_init();
            thread_rf_init();
            meshnetwork_up();
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
#endif
