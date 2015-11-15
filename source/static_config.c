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

#include <string.h>
#include "nanostack-border-router/config_def.h"

const char *cfg_string(conf_t *conf, const char *key, const char *default_value)
{
	for (;(conf && conf->name);conf++) {
		if (0 == strcmp(conf->name, key)) {
			return conf->svalue;
		}
	}
	return default_value;
}

int	cfg_int(conf_t *conf, const char *key, int default_value)
{
	for (;(conf && conf->name);conf++) {
		if (0 == strcmp(conf->name, key)) {
			return conf->ivalue;
		}
	}
	return default_value;
}
