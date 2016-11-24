/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */

#ifndef MBED_CONF_APP_THREAD_BR

#ifndef CFG_PARSER_H
#define CFG_PARSER_H

#include "config_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

const char *cfg_string(conf_t *conf, const char *key, const char *default_value);
int cfg_int(conf_t *conf, const char *key, int default_value);

#ifdef __cplusplus
}
#endif
#endif /* CFG_PARSER_H */
#endif /* MBED_CONF_APP_THREAD_BR */