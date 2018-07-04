/*
 * Copyright (c) 2018 ARM Limited. All rights reserved.
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


#include "mbed.h"
#include "Nanostack.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Application configuration values from json */
#define LOWPAN     1
#define THREAD     2
#define MESH_HEAP       3
#define MESH_SD_CARD    4

/* At the moment, Thread builds using K64F support NVM */
#if MBED_CONF_APP_MESH_MODE == THREAD && defined(TARGET_K64F)

#include "LittleFileSystem.h"
#include "SDBlockDevice.h"
#include "HeapBlockDevice.h"
#include "ns_file_system.h"
#include "mbed_trace.h"

#define TRACE_GROUP "mnvm"

/* By default use HEAP as NVM storage, comment macro in order to use SD card */
#ifdef MBED_CONF_APP_STORAGE_DEVICE
#if MBED_CONF_APP_STORAGE_DEVICE == MESH_HEAP
// Use Heap block device
#define USE_HEAP_BLOCK_DEVICE
#elif  MBED_CONF_APP_STORAGE_DEVICE == MESH_SD_CARD
// Use SD CARD - lack of USE_HEAP_BLOCK_DEVICE selects SD_CARD
#endif

#endif /* MBED_CONF_APP_STORAGE_OPTION */

LittleFileSystem *fs;
BlockDevice *bd;

void mesh_nvm_initialize()
{
    fs = new LittleFileSystem("fs");
#ifdef USE_HEAP_BLOCK_DEVICE
    const char *bd_info = "NVM: Heap";
    bd = new HeapBlockDevice(16 * 512, 512);
#else
    const char *bd_info = "NVM: SD";
    bd = new SDBlockDevice(MBED_CONF_SD_SPI_MOSI, MBED_CONF_SD_SPI_MISO, MBED_CONF_SD_SPI_CLK, MBED_CONF_SD_SPI_CS);
#endif

     tr_debug("%s", bd_info);
    int mount_status = fs->mount(bd);
    if (mount_status) {
        tr_warning("mount error: %d, trying format...", mount_status);
        mount_status = fs->reformat(bd);
        tr_info("reformat %s (%d)", mount_status ? "failed" : "OK", mount_status);
    }

    if (!mount_status) {
        Nanostack::get_instance(); // ensure Nanostack is initialised
        ns_file_system_set_root_path("/fs/");
        // Should be like: Nanostack::get_instance().file_system_set_root_path("/fs/");
    }
}

#else /* MBED_CONF_APP_MESH_TYPE == MESH_THREAD && defined(TARGET_K64F) */
void mesh_nvm_initialize()
{
    /* NVM not supported */
}
#endif  /* MBED_CONF_APP_MESH_TYPE == MESH_THREAD && defined(TARGET_K64F) */

#ifdef __cplusplus
}
#endif

