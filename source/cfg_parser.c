/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */


#ifndef MBED_CONF_APP_THREAD_BR
 
#include <string.h>
#include "nanostack-border-router/cfg_parser.h"

const char *cfg_string(conf_t *conf, const char *key, const char *default_value)
{
    for (; (conf && conf->name); conf++) {
        if (0 == strcmp(conf->name, key)) {
            return conf->svalue;
        }
    }
    return default_value;
}

int cfg_int(conf_t *conf, const char *key, int default_value)
{
    for (; (conf && conf->name); conf++) {
        if (0 == strcmp(conf->name, key)) {
            return conf->ivalue;
        }
    }
    return default_value;
}
#endif