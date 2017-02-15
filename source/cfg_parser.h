/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 */
#ifndef CFG_PARSER_H
#define CFG_PARSER_H

typedef struct conf_t {
    const char *name;
    const char *svalue;
    const int ivalue;
} conf_t;

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

extern conf_t *global_config;

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
