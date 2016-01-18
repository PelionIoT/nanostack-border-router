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

/*
 * Simple parser for configuration files
 * Author: Seppo Takalo
 *
 * Allowed fields are
 *  - comments
 *  - strings.
 *  - numbers.
 *  - byte arrays
 *
 * <value_name> = ["<string>" | <number> | <array>]
 * <number> := [<hex>|<decimal>]
 * <array> := '[' <number> [, <number ..] ']'
 *
 * Example of such file:
 * #comment. all after # is ignored
 * file = "something.txt"
 * number = 42.2
 * hex_number = 0xABBA
 * array = [ 0xFF, 0xAB, 255]
 *
 */

#ifndef CFG_PARSER_H
#define CFG_PARSER_H

#include "config_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct conf_t conf_t;

conf_t *cfg_parse(const char *file);
void cfg_free(conf_t *conf);
void cfg_dump(conf_t *conf);

const char *cfg_string(conf_t *conf,	const char *key, const char *default_value);
int	cfg_int(conf_t *conf,		const char *key, int default_value);
double cfg_double(conf_t *conf,	const char *key, double default_value);

#ifdef __cplusplus
}
#endif
#endif /* CFG_PARSER_H */
