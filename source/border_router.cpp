/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#include <string.h>


#include "mbed.h"
#include "nsdynmemLIB.h"
#include "nanostack-border-router/borderrouter_tasklet.h"
#include "sal-nanostack-driver-k64f-eth/k64f_eth_nanostack_port.h"
#include "sal-stack-nanostack-slip/Slip.h"

#ifdef  MBED_CONF_APP_DEBUG_TRACE
#if MBED_CONF_APP_DEBUG_TRACE == 1
#define APP_TRACE_LEVEL TRACE_ACTIVE_LEVEL_DEBUG
#else
#define APP_TRACE_LEVEL TRACE_ACTIVE_LEVEL_INFO
#endif
#endif //MBED_CONF_APP_DEBUG_TRACE

#include "ns_hal_init.h"
#include "cmsis_os.h"
#include "arm_hal_interrupt.h"


#include "ns_trace.h"

#define TRACE_GROUP "app"
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define APP_DEFINED_HEAP_SIZE MBED_CONF_APP_HEAP_SIZE


static uint8_t app_stack_heap[APP_DEFINED_HEAP_SIZE];
static uint8_t mac[6] = {0};

static SlipMACDriver *pslipmacdriver;
static DigitalOut led1(LED1);

static Ticker led_ticker;

static void app_heap_error_handler(heap_fail_t event);

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

/**
 * \brief Initializes the SLIP MAC backhaul driver.
 * This function is called by the border router module.
 */
void backhaul_driver_init(void (*backhaul_driver_status_cb)(uint8_t, int8_t))
{
    const char *driver;

    driver = STR(MBED_CONF_APP_BACKHAUL_DRIVER);

    if (strcmp(driver, "SLIP") == 0) {
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
    } else if (strcmp(driver, "ETH") == 0) {
        tr_info("Using ETH backhaul driver...");
        arm_eth_phy_device_register(mac, backhaul_driver_status_cb);
        return;
    }

    tr_error("Unsupported backhaul driver: %s", driver);
}

/**
 * \brief The entry point for this application.
 * Sets up the application and starts the border router module.
 */

void app_start(int, char **)
{
    ns_hal_init(app_stack_heap, APP_DEFINED_HEAP_SIZE, app_heap_error_handler, 0);

    trace_init(); // set up the tracing library
    set_trace_print_function(trace_printer);
    set_trace_config(TRACE_MODE_COLOR | APP_TRACE_LEVEL | TRACE_CARRIAGE_RETURN);

    const char *mac_src;

    mac_src = STR(MBED_CONF_APP_BACKHAUL_MAC_SRC);

    if (strcmp(mac_src, "BOARD") == 0) {
        /* Setting the MAC Address from UID.
         * Takes UID Mid low and UID low and shuffles them around. */
        mbed_mac_address((char *)mac);
    } else if (strcmp(mac_src, "CONFIG") == 0) {
        const uint8_t mac48[] = MBED_CONF_APP_BACKHAUL_MAC;
        for (uint32_t i = 0; i < sizeof(mac); ++i) {
            mac[i] = mac48[i];
        }
    }

    led_ticker.attach_us(toggle_led1, 500000);
    tr_info("Starting K64F border router...");
    border_router_start();
}

int main(int argc, char **argv)
{
    app_start(argc, argv);
}


/**
 * \brief Error handler for errors in dynamic memory handling.
 */
static void app_heap_error_handler(heap_fail_t event)
{
    tr_error("Dyn mem error %x", (int8_t)event);

    switch (event) {
        case NS_DYN_MEM_NULL_FREE:
            break;
        case NS_DYN_MEM_DOUBLE_FREE:
            break;
        case NS_DYN_MEM_ALLOCATE_SIZE_NOT_VALID:
            break;
        case NS_DYN_MEM_POINTER_NOT_VALID:
            break;
        case NS_DYN_MEM_HEAP_SECTOR_CORRUPTED:
            break;
        case NS_DYN_MEM_HEAP_SECTOR_UNITIALIZED:
            break;
        default:
            break;
    }

    while (1);
}
