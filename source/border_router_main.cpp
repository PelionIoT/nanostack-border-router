/*
 * Copyright (c) 2016-2019 ARM Limited. All rights reserved.
 */

#include <string.h>


#include "mbed.h"
#include "borderrouter_tasklet.h"
#include "drivers/eth_driver.h"
#include "sal-stack-nanostack-slip/Slip.h"

#include "Nanostack.h"
#include "NanostackEthernetInterface.h"
#include "MeshInterfaceNanostack.h"
#include "EMACInterface.h"
#include "EMAC.h"

#undef ETH
#undef SLIP
#undef EMAC
#undef CELL
#define ETH 1
#define SLIP 2
#define EMAC 3
#define CELL 4

#if MBED_CONF_APP_BACKHAUL_DRIVER == CELL
#include "NanostackPPPInterface.h"
#include "PPPInterface.h"
#include "PPP.h"
#endif

#ifdef  MBED_CONF_APP_DEBUG_TRACE
#if MBED_CONF_APP_DEBUG_TRACE == 1
#define APP_TRACE_LEVEL TRACE_ACTIVE_LEVEL_DEBUG
#else
#define APP_TRACE_LEVEL TRACE_ACTIVE_LEVEL_INFO
#endif
#endif //MBED_CONF_APP_DEBUG_TRACE

#include "ns_hal_init.h"
#include "mesh_system.h"
#include "cmsis_os.h"
#include "arm_hal_interrupt.h"
#include "nanostack_heap_region.h"

#include "mbed_trace.h"
#define TRACE_GROUP "app"

#define BOARD 1
#define CONFIG 2
#if MBED_CONF_APP_BACKHAUL_MAC_SRC == BOARD
static uint8_t mac[6];
#elif MBED_CONF_APP_BACKHAUL_MAC_SRC == CONFIG
static const uint8_t mac[] = MBED_CONF_APP_BACKHAUL_MAC;
#else
#error "MAC address not defined"
#endif

static DigitalOut led1(MBED_CONF_APP_LED);

static Ticker led_ticker;

static void toggle_led1()
{
    led1 = !led1;
}

/**
 * \brief Prints string to serial (adds CRLF).
 */
static void trace_printer(const char *str)
{
    printf("%s\n", str);
}

#if MBED_CONF_APP_BACKHAUL_DRIVER == EMAC
static void (*emac_actual_cb)(uint8_t, int8_t);
static int8_t emac_driver_id;
static void emac_link_cb(bool up)
{
    if (emac_actual_cb) {
        emac_actual_cb(up, emac_driver_id);
    }
}
#endif

/**
 * \brief Initializes the MAC backhaul driver.
 * This function is called by the border router module.
 */
void backhaul_driver_init(void (*backhaul_driver_status_cb)(uint8_t, int8_t))
{
// Values allowed in "backhaul-driver" option
#if MBED_CONF_APP_BACKHAUL_DRIVER == SLIP
    SlipMACDriver *pslipmacdriver;
    int8_t slipdrv_id = -1;
#if defined(MBED_CONF_APP_SLIP_HW_FLOW_CONTROL)
    pslipmacdriver = new SlipMACDriver(SERIAL_TX, SERIAL_RX, SERIAL_RTS, SERIAL_CTS);
#else
    pslipmacdriver = new SlipMACDriver(SERIAL_TX, SERIAL_RX);
#endif

    if (pslipmacdriver == NULL) {
        tr_error("Unable to create SLIP driver");
        return;
    }

    tr_info("Using SLIP backhaul driver...");

#ifdef MBED_CONF_APP_SLIP_SERIAL_BAUD_RATE
    slipdrv_id = pslipmacdriver->Slip_Init(mac, MBED_CONF_APP_SLIP_SERIAL_BAUD_RATE);
#else
    tr_warning("baud rate for slip not defined");
#endif

    if (slipdrv_id >= 0) {
        backhaul_driver_status_cb(1, slipdrv_id);
        return;
    }

    tr_error("Backhaul driver init failed, retval = %d", slipdrv_id);
#elif MBED_CONF_APP_BACKHAUL_DRIVER == EMAC
#undef EMAC
    tr_info("Using EMAC backhaul driver...");
    NetworkInterface *net = NetworkInterface::get_default_instance();
    if (!net) {
        tr_error("Default network interface not found");
        exit(1);
    }
    EMACInterface *emacif = net->emacInterface();
    if (!emacif) {
        tr_error("Default interface is not EMAC-based");
        exit(1);
    }
    EMAC &emac = emacif->get_emac();
    Nanostack::EthernetInterface *ns_if;
#if MBED_CONF_APP_BACKHAUL_MAC_SRC == BOARD
    /* Let the core code choose address - either from board or EMAC (for
     * ETH and SLIP we pass in the board address already in mac[]) */
    nsapi_error_t err = Nanostack::get_instance().add_ethernet_interface(emac, true, &ns_if);
    /* Read back what they chose into our mac[] */
    ns_if->get_mac_address(mac);
#else
    nsapi_error_t err = Nanostack::get_instance().add_ethernet_interface(emac, true, &ns_if, mac);
#endif
    if (err < 0) {
        tr_error("Backhaul driver init failed, retval = %d", err);
    } else {
        emac_actual_cb = backhaul_driver_status_cb;
        emac_driver_id = ns_if->get_driver_id();
        emac.set_link_state_cb(emac_link_cb);
    }
#elif MBED_CONF_APP_BACKHAUL_DRIVER == CELL
    tr_info("Using CELLULAR backhaul driver...");
    /* Creates PPP service and onboard network stack already here for cellular
     * connection to be able to override the link state changed callback */
    PPP *ppp = &PPP::get_default_instance();
    if (!ppp) {
        tr_error("PPP not found");
        exit(1);
    }
    OnboardNetworkStack *stack = &OnboardNetworkStack::get_default_instance();
    if (!stack) {
        tr_error("Onboard network stack not found");
        exit(1);
    }
    OnboardNetworkStack::Interface *interface;
    if (stack->add_ppp_interface(*ppp, true, &interface) != NSAPI_ERROR_OK) {
        tr_error("Cannot add PPP interface");
        exit(1);
    }
    Nanostack::PPPInterface *ns_if = static_cast<Nanostack::PPPInterface *>(interface);
    ns_if->set_link_state_changed_callback(backhaul_driver_status_cb);

    // Cellular interface configures it to PPP service and onboard stack created above
    CellularInterface *net = CellularInterface::get_default_instance();
    if (!net) {
        tr_error("Default cellular interface not found");
        exit(1);
    }
    net->set_default_parameters(); // from cellular nsapi .json configuration
    net->set_blocking(false);
    if (net->connect() != NSAPI_ERROR_OK) {
        tr_error("Connect failure");
        exit(1);
    }
#elif MBED_CONF_APP_BACKHAUL_DRIVER == ETH
    tr_info("Using ETH backhaul driver...");
    arm_eth_phy_device_register(mac, backhaul_driver_status_cb);
    return;
#else
#error "Unsupported backhaul driver"
#endif
#undef ETH
#undef SLIP
#undef EMAC
#undef CELL
}


void appl_info_trace(void)
{
    tr_info("Starting NanoStack Border Router...");
    tr_info("Build date: %s %s", __DATE__, __TIME__);
#ifdef MBED_MAJOR_VERSION
    tr_info("Mbed OS: %d", MBED_VERSION);
#endif

#if defined(MBED_SYS_STATS_ENABLED)
    mbed_stats_sys_t stats;
    mbed_stats_sys_get(&stats);

    /* ARM = 1, GCC_ARM = 2, IAR = 3 */
    tr_info("Compiler ID: %d", stats.compiler_id);
    /* Compiler versions:
       ARM: PVVbbbb (P = Major; VV = Minor; bbbb = build number)
       GCC: VVRRPP  (VV = Version; RR = Revision; PP = Patch)
       IAR: VRRRPPP (V = Version; RRR = Revision; PPP = Patch)
    */
    tr_info("Compiler Version: %d", stats.compiler_version);
#endif
}

/**
 * \brief The entry point for this application.
 * Sets up the application and starts the border router module.
 */
int main(int argc, char **argv)
{
    mbed_trace_init(); // set up the tracing library
    mbed_trace_print_function_set(trace_printer);
    mbed_trace_config_set(TRACE_MODE_COLOR | APP_TRACE_LEVEL | TRACE_CARRIAGE_RETURN);

    // Have to let mesh_system do net_init_core in case we use
    // Nanostack::add_ethernet_interface()
    mesh_system_init();

    nanostack_heap_region_add();

#if MBED_CONF_APP_BACKHAUL_MAC_SRC == BOARD
    mbed_mac_address((char *)mac);
#endif

    if (MBED_CONF_APP_LED != NC) {
        led_ticker.attach_us(toggle_led1, 500000);
    }
    border_router_tasklet_start();
}
