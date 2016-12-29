# Nanostack Border Router

Nanostack Border Router is a generic mbed border router implementation that provides the main 6LoWPAN ND or Thread border router logic usable to any 3rd party application. An example of such a application is [FRDM-K64F border router](https://github.com/ARMmbed/k64f-border-router-private), which can be used to build a 6LoWPAN ND or Thread border router.

The steps involved in porting a target platform are:

- Creating an mbed OS application
- Selecting the target platform
- Installing dependencies
- Adding source files
- Building and flashing

### Creating a border router application

Working with ARM mbed OS is fairly straightforward as most of the bits you need to develop an application are already in place for you. You can find all the related information from the [ARM mbed OS User Guide](https://docs.mbed.com/docs/mbed-os-handbook/en/).

### Selecting the target platform

Target platform is the hardware on which the border router runs. There are hundreds of target platforms already available for you out of the box, for example [FRDM-K64F](https://www.mbed.com/en/development/hardware/boards/nxp/frdm_k64f/). If you wish to write your own target, follow the instructions in [Adding target support to mbed OS 5](https://docs.mbed.com/docs/mbed-os-handbook/en/5.3/advanced/porting_guide/).


### Installing dependencies

To install dependencies for your application:

```shell
# To include a module as a dependency
$ mbed add "library URL"
```

For your border router, you may need the following modules (dependencies):

- Nanostack Border Router (this repository).
- Backhaul drivers, for example [Ethernet](https://github.com/ARMmbed/sal-nanostack-driver-k64f-eth) or [SLIP](https://github.com/ARMmbed/sal-stack-nanostack-slip).
- Radio driver, for example [Atmel IEEE 802.15.4 RF driver](https://github.com/ARMmbed/atmel-rf-driver).
- [6LowPAN networking stack (Nanostack)](https://github.com/ARMmbed/sal-stack-nanostack).

The diagram below shows a conceptual model for the [FRDM-K64F border router](https://github.com/ARMmbed/k64f-border-router-private) application, which is similar to your application.

![](/images/BorderRouter.png) 

### Adding source files to your project

After creating a new mbed project, you have an initial directory structure and project files in your project directory. The header files are located under the sub-directory with the same name as your project directory. The source files are located in the `source` directory.

Your border router application must implement at least the following two routines:

- `backhaul_driver_init()`
- `app_start()`

The former is needed by Nanostack Border Router (this repository) and the later is needed by mbed OS. All mbed OS applications start with the function `app_start()` (equivalent to `main()`). The purpose of `backhaul_driver_init()` is to register a backhaul interface to the networking stack. The driver for the backhaul interface must implement a routine that performs the registration of the driver. The [Device Driver API](https://docs.mbed.com/docs/arm-ipv66lowpan-stack/en/latest/driver_api/) of the ARM 6LoWPAN ND stack (*Nanostack*) provides `arm_net_phy_register()` routine to register the interface:

```C
int8_t arm_net_phy_register(phy_device_driver_s *phy_driver);
```
An example of a routine performing the registration of an Ethernet interface:

```C
static phy_device_driver_s eth_device_driver;

void arm_eth_phy_device_register(uint8_t *mac_ptr, void (*driver_status_cb)(uint8_t, int8_t)) {

    if (eth_interface_id < 0) {

        eth_device_driver.PHY_MAC = mac_ptr; 
        eth_device_driver.address_write = &arm_eth_phy_k64f_address_write;
        eth_device_driver.driver_description = "ETH";
        eth_device_driver.link_type = PHY_LINK_ETHERNET_TYPE;
        eth_device_driver.phy_MTU = 0;
        eth_device_driver.phy_header_length = 0;
        eth_device_driver.phy_tail_length = 0;
        eth_device_driver.state_control = &arm_eth_phy_k64f_interface_state_control;
        eth_device_driver.tx = &arm_eth_phy_k64f_tx;
        eth_interface_id = arm_net_phy_register(&eth_device_driver);
        driver_readiness_status_callback = driver_status_cb;

        if (eth_interface_id < 0){
            tr_error("Ethernet Driver failed to register with Stack. RetCode=%i", eth_driver_enabled);
            driver_readiness_status_callback(0, eth_interface_id);
            return;
        }
    }

    if (!eth_driver_enabled) {
        int8_t ret = k64f_eth_initialize();
        if (ret==-1) {
            tr_error("Failed to Initialize Ethernet Driver.");
            driver_readiness_status_callback(0, eth_interface_id);
        } else {
            tr_info("Ethernet Driver Initialized.");
            eth_driver_enabled = true;
            driver_readiness_status_callback(1, eth_interface_id);
        }
    }
}

```

In the `app_start()` function, you implement your application logic and start the border router module (provided in this repository) by calling the `border_router_start()` function.

```C
tr_info("Starting border router...");
border_router_start();
```

You also need to initialize the memory heap depending on the memory available on your hardware. For example:

```C
/* Se the heap size to ~32 KB */
static uint8_t app_stack_heap[32500];

/* Structure defining memory related errors */
typedef enum {
    NS_DYN_MEM_NULL_FREE,               /**< ns_dyn_mem_free(), NULL pointer free [obsolete - no longer faulted]  */
    NS_DYN_MEM_DOUBLE_FREE,             /**< ns_dyn_mem_free(), Possible double pointer free */
    NS_DYN_MEM_ALLOCATE_SIZE_NOT_VALID, /**< Allocate size is 0 or smaller or size is bigger than max heap size  */
    NS_DYN_MEM_POINTER_NOT_VALID,       /**< ns_dyn_mem_free(), try to free pointer which not at heap sector */
    NS_DYN_MEM_HEAP_SECTOR_CORRUPTED,   /**< Heap system detect sector corruption */
    NS_DYN_MEM_HEAP_SECTOR_UNITIALIZED  /**< ns_dyn_mem_free(), ns_dyn_mem_temporary_alloc() or ns_dyn_mem_alloc() called before ns_dyn_mem_init() */
} heap_fail_t;

/* Initialise dynamic memory
app_heap_error_handler() is the callback function if any memory related error takes place */
ns_dyn_mem_init(app_stack_heap, APP_DEFINED_HEAP_SIZE, app_heap_error_handler, 0);
``` 

### Configuring Nanostack Border Router

Applications using Nanostack Border Router need to use a `.json` file for the configuration. The example configurations can be found in [K64f-border-router configs] (https://github.com/ARMmbed/k64f-border-router-private/tree/master/configs).
The backhaul related configuration options are explained here:

| Field                               | Description                                                   |
|-------------------------------------|---------------------------------------------------------------|
| backhaul-dynamic-bootstrap          | Defines whether the manually configured backhaul prefix and default route are used, or whether they are learnt automatically via the IPv6 neighbor discovery. False means static and true means automatic configuration. |
| backhaul-prefix                     | The IPv6 prefix (64 bits) assigned to and advertised on the backhaul interface. Example format: `fd00:1:2::` |
| backhaul-default-route              | The default route (prefix and prefix length) where packets should be forwarded on the backhaul device, default: `::/0`. Example format: `fd00:a1::/10` |
| backhaul-next-hop                   | The next-hop value for the backhaul default route; should be a link-local address of a neighboring router, default: empty (on-link prefix). Example format: `fe80::1` |

The following parameters are only used in the 6LoWPAN ND border router.

| Field                               | Description                                                   |
|-------------------------------------|---------------------------------------------------------------|
| prefix-from-backhaul                | Whether or not the same prefix on the backhaul interface should also be used on the mesh network side. This option can be used when backhaul-dynamic-bootstrap is set to true. Note, that in the Thread network it is enough when the backhaul-dynamic-bootstrap parameter is set.
| security-mode                       | The 6LoWPAN mesh network traffic (link layer) can be protected with the Private Shared Key (PSK) security mode, allowed values: `NONE` and `PSK`. |
| psk-key                             | 16 bytes long private shared key to be used when the security mode is PSK. Example format (hexadecimal byte values separated by commas inside brackets): `{0x00, ..., 0x0f}` |
| multicast-addr                      | Multicast forwarding is supported by default. This defines the multicast address to which the border router application forwards multicast packets (on the backhaul and RF interface). Example format: `ff05::5` |

### Using Nanostack Border Router in your application

Here is a short recap of everything learnt above. To run a 6LoWPAN ND or Thread border router/gateway, your application needs to:

- Implement a callback for registering a backhaul network driver.
- Call the start function to get your border router up and running.

```C
/* Call this function after your application has been initialised */
void border_router_start(void);

/* Implement this function; backhaul_driver_status_cb() must be called by your app or the backhaul driver */
void backhaul_driver_init(void (*backhaul_driver_status_cb)(uint8_t, int8_t));
```

To create a border router application using the Nanostack Border Router module:

1. Call `ns_dyn_mem_init()` to set the heap size and a handler for memory errors of your application.

2. Set up the Nanostack tracing library. [OPTIONAL]

    ```C
       /* Initialise the tracing library */
       trace_init(); 

       /* Define your printing function */
       set_trace_print_function(my_print_function);

       /* Configure trace output for your taste */
       set_trace_config(TRACE_CARRIAGE_RETURN | ...);
    ```
    
    **Note:** You must call these functions before you call any Nanostack Border Router functions. For the detailed description of the above Nanostack functions, read [the Nanostack documentation](https://docs.mbed.com/docs/arm-ipv66lowpan-stack/en/latest/).

3. Call the `start_border_router()` function. Nanostack will call your `backhaul_driver_init()` function to register your backhaul driver.

4. Start the backhaul driver and invoke the `backhaul_driver_status_cb()` callback (performed by your code or the backhaul driver code).

For a complete application using Nanostack Border Router, read the [FRDM-K64F border router](https://github.com/ARMmbed/k64f-border-router-private) documentation.

#### The routing protocol RPL (6LoWPAN ND)

Nanostack Border Router uses [RPL](https://tools.ietf.org/html/rfc6550) as the routing protocol on the mesh network side (RF interface). Currently, only the `grounded/non-storing` operation mode is supported.

Nanostack Border Router offers the following configuration options for RPL:

| Field                               | Description                                             |
|-------------------------------------|---------------------------------------------------------|
| rpl-instance-id                     | The RPL instance ID value that identifies the RPL instance, default: 1 |
| rpl-idoublings                      | RPL Trickle parameter: DIOIntervalDoublings value, default: 12 |
| rpl-imin                            | RPL Trickle parameter: DIOIntervalMin value, default: 9 |
| rpl-k                               | RPL Trickle parameter: the redundacy constant k, default: 10 |
| rpl-max-rank-inc                    | Maximum rank increase value, default: 2048|
| rpl-min-hop-rank-inc                | Minimum rank increase value, default: 128 |
| rpl-default-lifetime                | Default lifetime for the RPL routes, default: 64 |
| rpl-lifetime-unit                   | The value of the unit that describes the lifetime (in seconds), default: 60 |
| rpl-pcs                             | The number of bits that may be allocated to the path control field. |
| rpl-ocp                             | The Objective Function (OF) to use, values: 1=OF0 (default), 2=MRHOF |
