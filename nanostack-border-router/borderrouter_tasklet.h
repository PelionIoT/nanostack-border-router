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

#ifndef BORDERROUTER_TASKLET_H
#define BORDERROUTER_TASKLET_H

#include "ns_types.h"
#include "eventOS_event.h"
#include "net_rpl.h"

/*
 * \struct rpl_setup_info_t
 * \brief Border Router RPL setup data base.
 */
typedef struct
{
  uint8_t   DODAG_ID[16];   /**< RPL DODAG ID */
  uint8_t   rpl_instance_id;  /**< RPL INTANCE ID */
  uint8_t   rpl_setups;     /**< RPL Setup (MOP, DODAG Pref) */
}rpl_setup_info_t;

#define NR_BACKHAUL_INTERFACE_PHY_DRIVER_READY 2
#define NR_BACKHAUL_INTERFACE_PHY_DOWN  3

extern border_router_setup_s br;
extern rpl_setup_info_t rpl_setup_info;
extern dodag_config_t dodag_config;
extern uint8_t br_def_prefix[16];
extern const uint8_t *networkid;
extern uint8_t br_ipv6_prefix[16];
extern net_6lowpan_link_layer_sec_mode_e link_security_mode;
extern net_tls_cipher_e pana_security_suite;
extern net_link_layer_psk_security_info_s link_layer_psk;

#ifdef __cplusplus
extern "C"
{
#endif
/**
 * Border Router tasklet.
 */
void borderrouter_tasklet(arm_event_s *event);

/**
 * Init Border Router Backhaul interface Drivers.
 */
void main_borderrouter_driver_init(void (*backhaul_driver_status_cb)(uint8_t,int8_t));

/**
 * Allocate 6LoWPAN  interface.
 */
int8_t main_6lowpan_interface_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BORDERROUTER_TASKLET_H */
