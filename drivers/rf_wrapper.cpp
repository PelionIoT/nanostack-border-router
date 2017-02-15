/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#include "rf_wrapper.h"

// Must be defined for next preprocessor tests to work
#define ATMEL       0
#define MCR20       1
#define NCS36510    2

#if MBED_CONF_APP_RADIO_TYPE == ATMEL
#include "NanostackRfPhyAtmel.h"
NanostackRfPhyAtmel rf_phy(ATMEL_SPI_MOSI, ATMEL_SPI_MISO, ATMEL_SPI_SCLK, ATMEL_SPI_CS,
                           ATMEL_SPI_RST, ATMEL_SPI_SLP, ATMEL_SPI_IRQ, ATMEL_I2C_SDA, ATMEL_I2C_SCL);
#elif MBED_CONF_APP_RADIO_TYPE == MCR20
#include "NanostackRfPhyMcr20a.h"
NanostackRfPhyMcr20a rf_phy(MCR20A_SPI_MOSI, MCR20A_SPI_MISO, MCR20A_SPI_SCLK, MCR20A_SPI_CS, MCR20A_SPI_RST, MCR20A_SPI_IRQ);
#endif //MBED_CONF_APP_RADIO_TYPE

extern "C" int8_t rf_device_register()
{
    return rf_phy.rf_register();
}

extern "C" void rf_read_mac_address(uint8_t *mac)
{
    rf_phy.get_mac_address(mac);
}
