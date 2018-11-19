/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */


#ifndef BORDERROUTER_TASKLET_H
#define BORDERROUTER_TASKLET_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Initializes the backhaul driver. MUST be implemented by the application.
 */
void backhaul_driver_init(void (*backhaul_driver_status_cb)(uint8_t, int8_t));

/**
* Trace application details
*/
void appl_info_trace(void);

/**
 * Initializes the border router module: loads configuration and
 * initiates bootstrap for the RF 6LoWPAN and backhaul interfaces.
 */
void border_router_tasklet_start(void);

#ifdef __cplusplus
}
#endif

#endif /* BORDERROUTER_TASKLET_H */
