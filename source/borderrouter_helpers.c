/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "ip6string.h"
#include "ns_types.h"
#include "common_functions.h"
#include "ns_trace.h"
#include "nsdynmemLIB.h"
#define TRACE_GROUP "app"

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

void print_memory_stats(void)
{
    const mem_stat_t *heap_info = ns_dyn_mem_get_mem_stat();
    if (heap_info) {
        tr_info(
            "Heap size: %lu, Reserved: %lu, Reserved max: %lu, Alloc fail: %lu"
            , (unsigned long)heap_info->heap_sector_size
            , (unsigned long)heap_info->heap_sector_allocated_bytes
            , (unsigned long)heap_info->heap_sector_allocated_bytes_max
            , (unsigned long)heap_info->heap_alloc_fail_cnt);
    }
}

