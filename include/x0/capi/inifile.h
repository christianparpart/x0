#pragma once

#include <sys/param.h>
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct s_x0_inifile x0_inifile_t;

x0_inifile_t* x0_inifile_create();
void x0_inifile_destroy(x0_inifile_t*);

int x0_inifile_load(x0_inifile_t* ini, const char* path);
int x0_inifile_save(x0_inifile_t* ini, const char* path);
void x0_inifile_clear(x0_inifile_t* ini);

size_t x0_inifile_section_count(x0_inifile_t* ini);
size_t x0_inifile_section_size(x0_inifile_t* ini, const char* title);
int x0_inifile_section_contains(x0_inifile_t* ini, const char* title);
void x0_inifile_section_remove(x0_inifile_t* ini, const char* title);

void x0_inifile_value_set(x0_inifile_t* ini, const char* title, const char* key, const char* value);
ssize_t x0_inifile_value_get(x0_inifile_t* ini, const char* title, const char* key, char* value, size_t size);

#if defined(__cplusplus)
}
#endif
