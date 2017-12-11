# Nanostack Border Router

Nanostack Border Router is a generic mbed border router implementation that provides the 6LoWPAN ND or Thread border router initialization logic.

Border router is a network gateway between a wireless 6LoWPAN mesh network and a backhaul network. It controls and relays traffic between the two networks. In a typical setup, a 6LoWPAN border router is connected to another router in the backhaul network (over Ethernet or a serial line) which in turn forwards traffic to/from the internet or a private company LAN, for instance.

![](images/br_role.png)

## Structure

This application runs on mbed OS and utilizes PHY drivers and Nanostack to form a border router.

![](images/structure.png)

The code layout is organized like this:

```
configs/                  Contains example configuration files
drivers/                  Contains PHY drivers
mbed-os/                  Contains mbed OS itself
source/                   Contains the application code
mbed_app.json             Build time configuration file (to be copied from the configs folder).
```

## Building

1. Clone this repository.
1. Run `mbed deploy`.
1. Select target platform.
1. Select toolchain.
1. Configure.
1. Build.

For example:

```
$ mbed deploy

$ mbed target K64F
 OR
$ mbed target NUCLEO_F429ZI

$ mbed toolchain GCC_ARM

$ mbed compile
```

## Selecting the target platform

The target platform is the hardware on which the border router runs. There are number of target platforms already available for you in the mbed OS.

If you wish to write your own target, follow the instructions in [Adding target support to mbed OS 5](https://docs.mbed.com/docs/mbed-os-handbook/en/latest/advanced/porting_guide/).

The border router requires backhaul and RF drivers to be provided for Nanostack. The backhaul is either SLIP or Ethernet. Currently, there are drivers for the following backhauls:

* [K64F Ethernet](https://github.com/ARMmbed/sal-nanostack-driver-k64f-eth)
* [NUCLEO_F429ZI Ethernet](https://github.com/ARMmbed/sal-nanostack-driver-stm32-eth)
* [SLIP driver](https://github.com/ARMmbed/sal-stack-nanostack-slip)

And following RF drivers:

* [Atmel AT86RF233](https://github.com/ARMmbed/atmel-rf-driver)
* [Atmel AT86RF212B](https://github.com/ARMmbed/atmel-rf-driver)
* [STM Spirit1](https://github.com/ARMmbed/stm-spirit1-rf-driver)
* [NXP MCR20A](https://github.com/ARMmbed/mcr20a-rf-driver)

The existing drivers are found in the `drivers/` folder. More drivers can be linked in.

See [Notes on different hardware](https://github.com/ARMmbed/mbed-os-example-mesh-minimal/blob/master/Hardware.md) to see known combinations that work.

## Configuring Nanostack Border Router

Applications using Nanostack Border Router need to use a `.json` file for the configuration. The example configurations can be found in the `configs/` folder. You can copy one of these files to the base folder of the application, rename as `mbed_app.json` and then compile.

### The backhaul configuration options

| Field                               | Description                                                   |
|-------------------------------------|---------------------------------------------------------------|
| `backhaul-dynamic-bootstrap`          | Defines whether the manually configured backhaul prefix and default route are used, or whether they are learnt automatically via the IPv6 neighbor discovery. False means static and true means automatic configuration. |
| `backhaul-prefix`                     | The IPv6 prefix (64 bits) assigned to and advertised on the backhaul interface. Example format: `fd00:1:2::` |
| `backhaul-default-route`              | The default route (prefix and prefix length) where packets should be forwarded on the backhaul device, default: `::/0`. Example format: `fd00:a1::/10` |
| `backhaul-next-hop`                   | The next-hop value for the backhaul default route; should be a link-local address of a neighboring router, default: empty (on-link prefix). Example format: `fe80::1` |
| `backhaul-mld`                        | Enable sending Multicast Listener Discovery reports to backhaul network when a new multicast listener is registered in mesh network. Values: true or false |

### 6LoWPAN ND border router options

| Field                               | Description                                                   |
|-------------------------------------|---------------------------------------------------------------|
| `security-mode`                       | The 6LoWPAN mesh network traffic (link layer) can be protected with the Private Shared Key (PSK) security mode, allowed values: `NONE` and `PSK`. |
| `psk-key`                             | A 16-bytes long private shared key to be used when the security mode is PSK. Example format (hexadecimal byte values separated by commas inside brackets): `{0x00, ..., 0x0f}` |
| `multicast-addr`                      | Multicast forwarding is supported by default. This defines the multicast address to which the border router application forwards multicast packets (on the backhaul and RF interface). Example format: `ff05::5` |
|`ra-router-lifetime`|Defines the router advertisement interval in seconds (default 1024 if left out).|
|`beacon-protocol-id`|Is used to identify beacons. This should not be changed (default 4 if left out).|

To learn more about 6LoWPAN and the configuration parameters, read the [6LoWPAN overview] (https://docs.mbed.com/docs/arm-ipv66lowpan-stack/en/latest/quick_start_intro/index.html).

See [configs/6lowpan_Atmel_RF.json](configs/6lowpan_Atmel_RF.json) for an example configuration file.

#### The routing protocol RPL (6LoWPAN ND)

Nanostack Border Router uses [RPL](https://tools.ietf.org/html/rfc6550) as the routing protocol on the mesh network side (RF interface) when in 6LoWPAN-ND mode. Currently, only the `grounded/non-storing` operation mode is supported.

Nanostack Border Router offers the following configuration options for RPL:

| Field                               | Description                                                   |
|-------------------------------------|---------------------------------------------------------------|
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

### Thread configuration

The Thread-specific parameters are listed below.

| Field                               | Description                                                   |
|-------------------------------------|---------------------------------------------------------------|
| `commissioning-dataset-timestamp`   | Used for updating the Thread network parameters. The device with the highest value propagates the parameters to the network (in the same Realm-Local scope). |
| `pan-id`                            | 2-byte Personal Area Network ID. |
| `extended-pan-id`                   | 8-byte value used to identify Thread networks in range. |
| `mesh-local-prefix`                 | ULA prefix used for communication within the Thread network. |
| `network-name`                      | A human-readable name for the network. |
| `pskc`                              | Pre-Shared Key for the Commissioner. |
| `pskd`                              | Pre-Shared Key for the device. |
| `thread-master-key`                 | A key used to derive security material for MAC and MLE protection. |


For the Thread Border Router, there are example configuration files for `SLIP` and `ETH` backhaul connectivity:

 * [configs/Thread_Atmel_RF.json](configs/Thread_Atmel_RF.json)
 * [configs/Thread_SLIP_Atmel_RF.json](configs/Thread_SLIP_Atmel_RF.json)

The [mbedtls_thread_config.h](source/mbedtls_thread_config.h) file configures mbed TLS for Thread use.

<span class="notes">**Note:** The configuration examples are for testing purposes only; do not use them for production or expose them.</span>


#### Backhaul connectivity

The Nanostack border router application can be connected to a backhaul network. This enables you to connect the devices in a 6LoWPAN mesh network to the internet or a private LAN. Currently, the application supports SLIP (IPv6 encapsulation over a serial line) and Ethernet backhaul connectivity.

```
"config": {
    "backhaul-driver": {
        "help": "options are ETH, SLIP",
        "value": "ETH"
    },
    "backhaul-mac-src": {
        "help": "Where to get EUI48 address. Options are BOARD, CONFIG",
        "value": "BOARD"
    },
    "backhaul-mac": "{0x02, 0x00, 0x00, 0x00, 0x00, 0x01}",
    "backhaul-dynamic-bootstrap": true,
    "backhaul-prefix": "\"fd00:300::\"",
    "backhaul-next-hop": "\"fe80::1\"",
    "backhaul-default-route": "\"::/0\"",
    .............
}
```

You can select your preferred option through the configuration file (field `backhaul-driver` in the `config` section). The value `SLIP` includes the SLIP driver, while the value `ETH` compiles the border router application with Ethernet backhaul support. You can define the MAC address on the backhaul interface manually (field `backhaul-mac-src` value `CONFIG`). Alternatively, you can use the MAC address provided by the development board (field `backhaul-mac-src` value `BOARD`). By default, the backhaul driver is set to `ETH` and the MAC address source is `BOARD`.

You can also set the backhaul bootstrap mode (field `backhaul-dynamic-bootstrap`). By default, the bootstrap mode is set to true, which means the autonomous mode. With the autonomous mode, the border router learns the prefix information automatically from an IPv6 gateway in the Ethernet/SLIP segment. When the parameter is set to false, it enables you to set up a manual configuration of `backhaul-prefix` and `backhaul-default-route`.

If you use the static bootstrap mode, you need to configure a default route on the backhaul interface to properly forward packets between the backhaul and the 6LoWPAN mesh network. In addition to this, you need to set a backhaul prefix. The static mode creates a site-local IPv6 network from which packets cannot be routed outside.

When using the autonomous mode in the 6LoWPAN ND configuration, you can set the `prefix-from-backhaul` option to `true` to use the same backhaul prefix on the mesh network side as well. This allows the mesh nodes to be directly connectable from the outside of the mesh network. In the Thread network, it is enough that `backhaul-dynamic-bootstrap` is set to true.


#### Note on the SLIP backhaul driver

If you are using a K64F board, you need to use the UART1 serial line of the board with the SLIP driver. See the `pins` section in the `mbed_app.json` configuration file. To use a different UART line, replace the `SERIAL_TX` and `SERIAL_RX` values with correct TX/RX pin names.

If you wish to use the hardware flow control, set the configuration field `slip_hw_flow_control` to true. By default, it is set to false. Before using hardware flow control, make sure that the other end of your SLIP interface can handle the flow control.

For the pin names of your desired UART line, refer to your development board's documentation.

An example configuration for the SLIP driver:

```
"target_overrides": {
    "K64F": {
        "SERIAL_TX": "PTE0",
        "SERIAL_RX": "PTE1",
        "SERIAL_CTS": "PTE2",
        "SERIAL_RTS": "PTE3"
    }
```

### Switching the RF shield

By default, the application uses an Atmel AT86RF233/212B RF driver. You can alternatively use any RF driver provided in the `drivers/` folder or link in your own driver. You can set the configuration for the RF driver in the `json` file.

To select the Atmel radio shield, use the following:

```
        "radio-type":{
            "help": "options are ATMEL, MCR20, SPIRIT1",
            "value": "ATMEL"
        },
```

To select the NXP radio shield, use the following:

```
        "radio-type":{
            "help": "options are ATMEL, MCR20, SPIRIT1",
            "value": "MCR20"
        },
```

To select the STM Spirit1 radio shield, use the following:

```
        "radio-type":{
            "help": "options are ATMEL, MCR20, SPIRIT1",
            "value": "SPIRIT1"
        },
```

In case you have choosen the STM Spirit1 Sub-1 GHz RF expansion board [X-NUCLEO-IDS01A4](https://github.com/ARMmbed/stm-spirit1-rf-driver), you need also to configure its MAC address in the `mbed_app.json` file, for example:

```json
    "target_overrides": {
        "*": {
            "spirit1.mac-address": "{0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7}"
        },
    }
```

<span class="notes">**Not**: This MAC address must be unique within the 6LoWPAN mesh network.</span>

After changing the radio shield, you need to recompile the application.

## Running the border router application

1. Find the  binary file `nanostack-border-router.bin` in the `BUILD` folder.
2. Copy the binary to the USB mass storage root of the development board. It is automatically flashed to the target MCU. When the flashing is complete, the board restarts itself. Press the **Reset** button of the development board if it does not restart automatically.
3. The program begins execution.
4. Open the [serial connection](#serial-connection-settings), for example PuTTY.

## Serial connection settings

Serial connection settings for the Nanorouter are as follows:

* Baud-rate = 115200
* Data bits = 8
* Stop bits = 1

If there is no input from the serial terminal, press the **Reset** button of the development board.

In the PuTTY main screen, save the session and click **Open**. This opens a console window showing debug messages from the application. If the console screen is blank, you may need to press the **Reset** button of the board to see the debug information. The serial output from the 6LoWPAN border router looks something like this in the console:

```
[INFO][app ]: Starting Nanostack border router...
[INFO][brro]: NET_IPV6_BOOTSTRAP_AUTONOMOUS
[INFO][app ]: Using ETH backhaul driver...
[INFO][Eth ]: Ethernet cable is connected.
[INFO][addr]: Tentative Address added to IF 1: fe80::ac41:dcff:fe37:72c4
[INFO][addr]: DAD passed on IF 1: fe80::ac41:dcff:fe37:72c4
[INFO][addr]: Tentative Address added to IF 1: 2001:999:21:9ce:ac41:dcff:fe37:72c4
[INFO][addr]: DAD passed on IF 1: 2001:999:21:9ce:ac41:dcff:fe37:72c4
[INFO][brro]: Backhaul bootstrap ready, IPv6 = 2001:999:21:9ce:ac41:dcff:fe37:72c4
[INFO][brro]: Backhaul interface addresses:
[INFO][brro]:    [0] fe80::ac41:dcff:fe37:72c4
[INFO][brro]:    [1] 2001:999:21:9ce:ac41:dcff:fe37:72c4
[INFO][addr]: Address added to IF 0: fe80::ff:fe00:face
[INFO][br  ]: BR nwk base ready for start
[INFO][br  ]: Refresh Contexts
[INFO][br  ]: Refresh Prefixs
[INFO][addr]: Address added to IF 0: 2001:999:21:9ce:0:ff:fe00:face
[INFO][addr]: Address added to IF 0: fe80::fec2:3d00:4:a0cd
[INFO][brro]: RF bootstrap ready, IPv6 = 2001:999:21:9ce:0:ff:fe00:face
[INFO][brro]: RF interface addresses:
[INFO][brro]:    [0] fe80::ff:fe00:face
[INFO][brro]:    [1] fe80::fec2:3d00:4:a0cd
[INFO][brro]:    [2] 2001:999:21:9ce:0:ff:fe00:face
[INFO][brro]: 6LoWPAN Border Router Bootstrap Complete.
```
