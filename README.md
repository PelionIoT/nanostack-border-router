# Nanostack Border Router
Nanostack Border Router is a yotta module that provides the 6LoWPAN mesh network (1 RF interface) and backhaul network (1 backhaul interface) connectivity. It is a building block for 3rd party border router applications that implement their own backhaul driver or use some 3rd party driver.

For an example of an application using the Nanostack Border Router module, see [FRDM-K64F border router](https://github.com/ARMmbed/k64f-border-router).

#### Configuring Nanostack Border Router
Applications using the Nanostack Border Router should use a `config.json` file for configuration. The file should contain a   *border-router* section under which the Nanostack Border Router specific configuration options are defined. The complete list of all configuration options can be found in the [config.json.example](config.json.example) file.

The minimum set of configuration options required are explained in the table below.

| Field                               | Description                                                   |
|-------------------------------------|---------------------------------------------------------------|
| backhaul-bootstrap-mode             | Defines whether the manually configured backhaul prefix and default route are used, or whether they are learnt automatically via the IPv6 neighbor discovery. Allowed values are `NET_IPV_BOOTSTRAP_STATIC` and `NET_IPV_BOOTSTRAP_AUTONOMOUS`. |
| backhaul-prefix                     | The IPv6 prefix (64 bits) assigned to and advertised on the backhaul interface. Example format: `fd00:1:2::` |
| backhaul-default-route              | The default route (prefix and prefix length) where packets should be forwarded on the backhaul device, default: `::/0`. Example format: `fd00:a1::/10` |
| backhaul-next-hop                   | The next-hop value for the backhaul default route; should be a link-local address of a neighboring router, default: empty (on-link prefix). Example format: `fe80::1` |
| rf-channel                          | The wireless (6LoWPAN mesh network) radio channel the border router application listens to. |
| security-mode                       | The 6LoWPAN mesh network traffic (link layer) can be protected with the Private Shared Key (PSK) security mode, allowed values: `NONE` and `PSK`. |
| psk-key                             | 16 bytes long private shared key to be used when the security mode is PSK. Example format (hexadecimal byte values separated by commas inside brackets): `{0x00, ..., 0x0f}` |
| multicast-addr                      | Multicast forwarding is supported by default. This defines the multicast address to which the border router application forwards multicast packets (on the backhaul and RF interface). Example format: `ff05::5` |

An example of a yotta configuration file `config.json`:

```json
{
  "border-router": {
	"security-mode": "PSK",
	"psk-key-id": 1,
	"psk-key": "{0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf}",
	"backhaul-bootstrap-mode": "NET_IPV6_BOOTSTRAP_STATIC",
	"backhaul-prefix": "fd00:abcd::",
	"backhaul-default-route": "::/0",
	"backhaul-next-hop": "fe80::1",
	"rf-channel": 11,
	"multicast-addr": "ff05::4"
  }
}
```

#### Using Nanostack Border Router in your application
To run a 6LoWPAN border router/gateway, your application needs to:

- Implement a callback for registering a backhaul network driver.
- Call a start function to get your border router up and running.

```C
/* Call this function after your application has been initialised */
void start_border_router(void);

/* Implement this function; backhaul_driver_status_cb() must be called by your app or the backhaul driver */
void backhaul_driver_init(void (*backhaul_driver_status_cb)(uint8_t, int8_t));
```

Steps to create a border router application using the Nanostack Border Router module:

1. Call `ns_dyn_mem_init()` to set the heap size and a handler for memory errors for your application.

2. Set up the Nanostack tracing library.
   ```C
   /* Initialize the tracing library */
   trace_init(); 

   /* Define your printing function */
   set_trace_print_function(my_print_function);

   /* Configure trace output for your taste */
   set_trace_config(TRACE_CARRIAGE_RETURN | ...);
   ```

   **Note**: These functions must be called before any Nanostack Border Router functions are called. For the detailed descriptions of the above Nanostack functions, pelase refer to [the Nanostack documentation](https://docs.mbed.com/docs/arm-ipv66lowpan-stack/en/latest/).

3. Call the `start_border_router()` function; Nanostack will call your `backhaul_driver_init()` function to register your backhaul driver.

4. Start the backhaul driver and invoke the `backhaul_driver_status_cb()` callback (performed by your code or the driver code).

For a complete application using Nanostack Border Router, please refer to [FRDM-K64F border router](https://github.com/ARMmbed/k64f-border-router).

#### The routing protocol RPL
Nanostack Border Router uses [RPL](https://tools.ietf.org/html/rfc6550) as the routing protocol on the mesh network side (RF interface). Currently, only the `grounded/non-storing` operation mode is supported.

Nanostack Border Router offers the following yotta configuration options for RPL:

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
