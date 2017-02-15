/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#ifndef CONFIG_DEF_H
#define CONFIG_DEF_H

typedef struct conf_t {
    const char *name;
    const char *svalue;
    const int ivalue;
} conf_t;

extern conf_t *global_config;
#endif /* CONFIG_DEF_H */
