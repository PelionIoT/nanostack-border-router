/*
 * Copyright (c) 2019, Pelion and affiliates.
 */

#include "nsdynmemLIB.h"

/* Enable nanostack extended heap only for specific targets and toolchains */
#if (MBED_CONF_APP_NANOSTACK_EXTENDED_HEAP == true)

#if defined(TARGET_K64F)
#define NANOSTACK_EXTENDED_HEAP_REGION_SIZE (60*1024)
#endif

#if defined(TARGET_NUCLEO_F429ZI)
#define NANOSTACK_EXTENDED_HEAP_REGION_SIZE (60*1024)
#endif

#if defined(TARGET_DISCO_F769NI)
#define NANOSTACK_EXTENDED_HEAP_REGION_SIZE (250*1024)
#endif

#if defined(__IAR_SYSTEMS_ICC__) || defined(__IAR_SYSTEMS_ASM__) || defined(__ICCARM__)
// currently - no IAR suport
#undef NANOSTACK_EXTENDED_HEAP_REGION_SIZE
#endif

#endif // MBED_CONF_APP_NANOSTACK_EXTENDED_HEAP


#ifdef NANOSTACK_EXTENDED_HEAP_REGION_SIZE

// Heap region for GCC_ARM, ARMCC and ARMCLANG
static uint8_t nanostack_extended_heap_region[NANOSTACK_EXTENDED_HEAP_REGION_SIZE];

void nanostack_heap_region_add(void)
{
    ns_dyn_mem_region_add(nanostack_extended_heap_region, NANOSTACK_EXTENDED_HEAP_REGION_SIZE);
}

#else // NANOSTACK_EXTENDED_HEAP_REGION_SIZE

void nanostack_heap_region_add(void)
{
}

#endif // NANOSTACK_EXTENDED_HEAP_REGION_SIZE
