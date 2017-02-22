#ifndef ETH_DRIVER_H
#define ETH_DRIVER_H

/**
 * Initialize Ethernet PHY driver and register itself to MAC.
 *
 * Must follow C calling conventions.
 *
 * \param mac_ptr Pointer to EIU-48 address
 * \param app_ipv6_init_cb Callback to receive link status and interface id
 */


extern "C" void arm_eth_phy_device_register(uint8_t *mac_ptr, void (*app_ipv6_init_cb)(uint8_t, int8_t));

#endif