#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../flow.h"
#define LOG(...) logmsg(__VA_ARGS__)
void *pkgi_create(const char*);void *pkgi_open(const char*);void pkgi_close(void*);
int pkgi_write(void*,const void*,uint32_t);int64_t pkgi_get_size(const char*);
void *pkgi_malloc(uint32_t);void pkgi_free(void*);
const char *pkgi_get_storage_device(void);int pkgi_mkdirs(const char*);
