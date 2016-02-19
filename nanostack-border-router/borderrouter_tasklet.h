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
 * Initializes the border router module: loads configuration and
 * initiates bootstrap for the RF 6LoWPAN and backhaul interfaces.
 */
void border_router_start(void);

#ifdef __cplusplus
}
#endif

#endif /* BORDERROUTER_TASKLET_H */
