/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */


#ifndef BORDERROUTER_HELPERS_H
#define BORDERROUTER_HELPERS_H

#ifdef __cplusplus
extern "C"
{
#endif

char *print_ipv6(const void *addr_ptr);
char *print_ipv6_prefix(const uint8_t *prefix, uint8_t prefix_len);
void print_memory_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* BORDERROUTER_HELPERS_H */
