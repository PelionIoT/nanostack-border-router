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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "ip6string.h"
#include "ns_types.h"

static char tmp_print_buffer[128] = {0};

char *print_ipv6(const void *addr_ptr)
{
    ip6tos(addr_ptr, tmp_print_buffer);
    return tmp_print_buffer;
}

char *print_ipv6_prefix(const uint8_t *prefix, uint8_t prefix_len)
{
    char *str = tmp_print_buffer;
    int retval;
    char tmp[40];
    uint8_t addr[16] = {0};

    if (prefix_len != 0) {
        if (prefix == NULL || prefix_len > 128) {
            return "<err>";
        }
        bitcopy(addr, prefix, prefix_len);
    }

    ip6tos(addr, tmp);
    retval = snprintf(str, 128, "%s/%u", tmp, prefix_len);
    if (retval <= 0) {
        return "";
    }
    return str;
}
