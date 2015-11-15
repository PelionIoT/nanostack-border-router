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

 #ifndef STATIC_CONFIG_H
 #define STATIC_CONFIG_H

static const char psk_key[] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF };
static const char tls_psk_key[] = {0xcf, 0xce, 0xcd, 0xcc, 0xcb, 0xca, 0xc9, 0xc8, 0xc7, 0xc6, 0xc5, 0xc4, 0xc3, 0xc2, 0xc1, 0xc0};
static const char short_addr_suffix[6] = { 0x00, 0x00, 0x00, 0xff, 0xfe, 0x00};

static conf_t static_config[] = {
	/* NAME,	STRING_VALUE,	INT_VALUE */
	{ "NAME",	"EmbeddedRouter",				0},
	{ "MODEL",	"NanoRouter3",				0},
	{ "MANUFACTURER","ARM",		0},

	{ "NETWORK_MODE",	"ND_WITH_MLE", 0 },
	{ "SECURITY_MODE",      "NONE",             0},
	{ "PANA_MODE",      "PSK",              0},
	{ "PSK_KEY",    psk_key,    0},
	{ "BR_PAN_ID", NULL, 0x0700},
	{ "NETWORKID",		"Network000000000",				0},

#ifdef NET_IPV6_BOOTSTRAP_MODE_S
	{ "PREFIX",					"2a01:348:2e0:1::",			0},
	{ "BACKHAUL_PREFIX",		"2a01:348:2e0:81::",			0},
#endif

	{ "NSP_PORT", NULL, 5683U},
	{ "NSP_LIFETIME", NULL, 120},
	{ "REGISTER_WITH_TEMPLATES", NULL, 0},

	{ "EDTLS_MODE", "NONE", 0},
	{ "EDTLS_PSK_KEY", "1234567890123456", 0},
	{ "EDTLS_PSK_KEY_ID", NULL, 0x0f0fU},

	{ "BR_SUBGHZ_CHANNEL", NULL, 1},
	{ "BR_2_4_GHZ_CHANNEL", NULL, 12},
	{ "BR_CHANNEL", NULL, 12},

	{ "BR_RPL_FLAGS", NULL, (BR_DODAG_PREF_7 | BR_DODAG_MOP_NON_STRORING)},
	{ "BR_RPL_INSTANCE_ID", NULL, 1},
	{ "BR_ND_ROUTE_LIFETIME", NULL, 1024},
	{ "BR_BEACON_PROTOCOL_ID", NULL, 4},

	{ "BR_DAG_DIO_INT_DOUB", NULL, 12},
	{ "BR_DAG_DIO_INT_MIN", NULL, 9},
	{ "BR_DAG_DIO_REDU", NULL, 10},
	{ "BR_DAG_MAX_RANK_INC", NULL, 16},
	{ "BR_DAG_MIN_HOP_RANK_INC", NULL, 128},
	{ "BR_LIFE_IN_SECONDS", NULL, 64},
	{ "BR_LIFETIME_UNIT", NULL, 60},
	{ "BR_DAG_SEC_PCS", NULL, 1},
	{ "BR_DAG_OCP", NULL, 1},

	{ "APP_SHORT_ADDRESS_SUFFIX", short_addr_suffix, 0},

	{ "TLS_PSK_KEY", tls_psk_key, 0},
	{ "TLS_PSK_KEY_ID", NULL,  0x30U},

	/* Array must end on NULL,NULL,0 field */
	{ NULL, NULL, 0}
};

conf_t *global_config = static_config;

#endif	//STATIC_CONFIG_H
