/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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

#ifndef MBED_CONFIG_H
#define MBED_CONFIG_H

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#ifndef MBED_CONF_APP_THREAD_BR

static const char psk_key[16] = MBED_CONF_APP_PSK_KEY;
static const char tls_psk_key[16] = MBED_CONF_APP_TLS_PSK_KEY;

static conf_t mbed_config[] = {
    /* NAME, STRING_VALUE, INT_VALUE */
    {"NAME", STR(MBED_CONF_APP_NAME), 0},
    {"MODEL", STR(MBED_CONF_APP_MODEL), 0},
    {"MANUFACTURER", STR(MBED_CONF_APP_MANUFACTURER), 0},
    {"NETWORK_MODE", STR(MBED_CONF_APP_NETWORK_MODE), 0 },
    {"SECURITY_MODE", STR(MBED_CONF_APP_SECURITY_MODE), 0},
    {"PANA_MODE", STR(MBED_CONF_APP_PANA_MODE), 0},
    {"PSK_KEY", psk_key, 0},
    {"PSK_KEY_ID", NULL, MBED_CONF_APP_PSK_KEY_ID},
    {"PAN_ID", NULL, MBED_CONF_APP_PAN_ID},
    {"NETWORK_ID", STR(MBED_CONF_APP_NETWORK_ID), 0},
    {"PREFIX", STR(MBED_CONF_APP_PREFIX), 0},
    {"BACKHAUL_PREFIX", STR(MBED_CONF_APP_BACKHAUL_PREFIX), 0},
    {"BACKHAUL_DEFAULT_ROUTE", STR(MBED_CONF_APP_BACKHAUL_DEFAULT_ROUTE), 0},
    {"BACKHAUL_NEXT_HOP", STR(MBED_CONF_APP_BACKHAUL_NEXT_HOP), 0},
    {"RF_CHANNEL", NULL, MBED_CONF_APP_RF_CHANNEL},
    {"RF_CHANNEL_PAGE", NULL, MBED_CONF_APP_RF_CHANNEL_PAGE},
    {"RF_CHANNEL_MASK", NULL, MBED_CONF_APP_RF_CHANNEL_MASK},
    {"RPL_INSTANCE_ID", NULL, MBED_CONF_APP_RPL_INSTANCE_ID},
    {"RPL_IDOUBLINGS", NULL, MBED_CONF_APP_RPL_IDOUBLINGS},
    {"RPL_K", NULL, MBED_CONF_APP_RPL_K},
    {"RPL_MAX_RANK_INC", NULL, MBED_CONF_APP_RPL_MAX_RANK_INC},
    {"RPL_MIN_HOP_RANK_INC", NULL, MBED_CONF_APP_RPL_MIN_HOP_RANK_INC},
    {"RPL_IMIN", NULL, MBED_CONF_APP_RPL_IMIN},
    {"RPL_DEFAULT_LIFETIME", NULL, MBED_CONF_APP_RPL_DEFAULT_LIFETIME},
    {"RPL_LIFETIME_UNIT", NULL, MBED_CONF_APP_RPL_LIFETIME_UNIT},
    {"RPL_PCS", NULL, MBED_CONF_APP_RPL_PCS},
    {"RPL_OCP", NULL, MBED_CONF_APP_RPL_OCP},
    {"RA_ROUTER_LIFETIME", NULL, MBED_CONF_APP_RA_ROUTER_LIFETIME},
    {"BEACON_PROTOCOL_ID", NULL, MBED_CONF_APP_BEACON_PROTOCOL_ID},
    {"TLS_PSK_KEY", tls_psk_key, 0},
    {"TLS_PSK_KEY_ID", NULL, MBED_CONF_APP_TLS_PSK_KEY_ID},
    {"BACKHAUL_BOOTSTRAP_MODE", NULL, MBED_CONF_APP_BACKHAUL_BOOTSTRAP_MODE},
    {"SHORT_MAC_ADDRESS", NULL, MBED_CONF_APP_SHORT_MAC_ADDRESS},
    {"MULTICAST_ADDR", STR(MBED_CONF_APP_MULTICAST_ADDR), 0},
    {"PREFIX_FROM_BACKHAUL", NULL, MBED_CONF_APP_PREFIX_FROM_BACKHAUL},
    /* Array must end on {NULL, NULL, 0} field */
    {NULL, NULL, 0}
};
conf_t *global_config = mbed_config;
#endif

#endif //MBED_CONFIG_H
