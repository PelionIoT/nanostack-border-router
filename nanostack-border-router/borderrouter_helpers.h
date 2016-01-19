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

#ifndef BORDERROUTER_HELPERS_H
#define BORDERROUTER_HELPERS_H

#ifdef __cplusplus
extern "C"
{
#endif

char *print_ipv6(const void *addr_ptr);
char *print_ipv6_prefix(const uint8_t *prefix, uint8_t prefix_len);

#ifdef __cplusplus
}
#endif

#endif /* BORDERROUTER_HELPERS_H */
