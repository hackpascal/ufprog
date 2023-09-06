/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Windows platform specific definitions
 */
#pragma once

#ifndef _UFPROG_OSDEF_H_
#define _UFPROG_OSDEF_H_

#include <stdio.h>
#include <stdarg.h>
#include <ufprog/common.h>

EXTERN_C_BEGIN

typedef int (*os_main_entry)(int argc, char *argv[]);

#if defined(WIN32)
#include <wchar.h>

#define PATH_SEP			'\\'
#define MODULE_SUFFIX			".dll"
#define os_strdup			_strdup
#define strcasecmp			_stricmp
#define strncasecmp			_strnicmp

typedef intptr_t ssize_t;

int os_main(os_main_entry entry, int argc, wchar_t *argv[]);

char *strcasestr(const char *s, const char *find);
size_t strlcpy(char *dst, const char *src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t dsize);
char * strndup(const char *str, size_t n);
ssize_t wgetdelim(wchar_t **bufptr, size_t *n, int delim, FILE *fp);
int vasprintf(char **strp, const char *fmt, va_list ap);
int asprintf(char **strp, const char *fmt, ...);

ufprog_status UFPROG_API os_vfprintf(FILE *f, const char *fmt, va_list args);
ufprog_status UFPROG_API os_fprintf(FILE *f, const char *fmt, ...);

static inline ufprog_status os_printf(const char *fmt, ...)
{
	ufprog_status ret;
	va_list args;

	if (!fmt)
		return UFP_INVALID_PARAMETER;

	va_start(args, fmt);
	ret = os_vfprintf(stdout, fmt, args);
	va_end(args);

	return ret;
}
#else
#define PATH_SEP			'/'
#define MODULE_SUFFIX			".so"
#define os_strdup			strdup

#define os_vfprintf			vfprintf
#define os_fprintf			fprintf
#define os_printf			printf

static inline int os_main(os_main_entry entry, int argc, char *argv[])
{
	return entry(argc, argv);
}
#endif

/* System-specific initialization */
ufprog_bool UFPROG_API os_init(void);

/* Program name */
const char *UFPROG_API os_prog_name(void);

/* System default logging callback */
void set_os_default_log_print(void);

/* System implementation of getline */
char *UFPROG_API os_getline_alloc(FILE *f);

/* Ctrl-C handler */
typedef ufprog_bool (UFPROG_API *ctrlc_handler)(void);
ufprog_bool UFPROG_API os_register_ctrlc_handler(ctrlc_handler handler);

/* Mutex */
typedef struct os_mutex_handle *mutex_handle;
ufprog_bool UFPROG_API os_create_mutex(mutex_handle *outmutex);
ufprog_bool UFPROG_API os_free_mutex(mutex_handle mutex);
ufprog_bool UFPROG_API os_mutex_lock(mutex_handle mutex);
ufprog_bool UFPROG_API os_mutex_unlock(mutex_handle mutex);

/* High-resolution timer */
uint64_t UFPROG_API os_get_timer_us(void);
void UFPROG_API os_udelay(uint64_t us);

/* Module related */
typedef struct os_module_handle *module_handle;

struct symbol_find_entry {
	const char *name;
	ufprog_bool found;
	void **psymbol;
};

#define FIND_MODULE(_name, _ptr)	{ .name = (_name), .psymbol = (void **)&(_ptr) }

ufprog_status UFPROG_API os_load_module(const char *module_path, module_handle *handle);
void UFPROG_API os_unload_module(module_handle module);
void *UFPROG_API os_find_module_symbol(module_handle module, const char *name);
ufprog_status UFPROG_API os_find_module_symbols(module_handle module, struct symbol_find_entry *list, size_t count,
						ufprog_bool full);

/* Filesystem related */
typedef struct os_file_handle *file_handle;
typedef struct os_file_mapping *file_mapping;

enum os_file_seek_method {
	FILE_SEEK_BEGIN,
	FILE_SEEK_CURR,
	FILE_SEEK_END,
};

typedef int (UFPROG_API *enum_file_cb)(void *priv, const char *base, const char *filename);

ufprog_bool UFPROG_API os_is_valid_filename(const char *filename);
ufprog_bool UFPROG_API os_mkdir_p(const char *path);
ufprog_bool UFPROG_API os_enum_file(const char *dir, ufprog_bool recursive, void *priv, enum_file_cb cb);
ufprog_status UFPROG_API os_open_file(const char *file, ufprog_bool read, ufprog_bool write, ufprog_bool trunc,
				      ufprog_bool create, file_handle *outhandle);
ufprog_bool UFPROG_API os_close_file(file_handle handle);
ufprog_bool UFPROG_API os_get_file_size(file_handle handle, uint64_t *retval);
ufprog_bool UFPROG_API os_set_file_pointer(file_handle handle, enum os_file_seek_method method, uint64_t distance,
					   uint64_t *retpointer);
ufprog_bool UFPROG_API os_set_end_of_file(file_handle handle);
ufprog_bool UFPROG_API os_read_file(file_handle handle, size_t len, void *buf, size_t *retlen);
ufprog_bool UFPROG_API os_write_file(file_handle handle, size_t len, const void *buf, size_t *retlen);

ufprog_status UFPROG_API os_open_file_mapping(const char *file, uint64_t size, size_t mapsize, ufprog_bool write,
					      ufprog_bool trunc, file_mapping *outmapping);
ufprog_bool UFPROG_API os_close_file_mapping(file_mapping mapping);
ufprog_bool UFPROG_API os_set_file_mapping_offset(file_mapping mapping, uint64_t offset, void **memory);
size_t UFPROG_API os_get_file_mapping_granularity(file_mapping mapping);
size_t UFPROG_API os_get_file_max_mapping_size(file_mapping mapping);
void *UFPROG_API os_get_file_mapping_memory(file_mapping mapping);
uint64_t UFPROG_API os_get_file_mapping_offset(file_mapping mapping);
size_t UFPROG_API os_get_file_mapping_size(file_mapping mapping);
file_handle UFPROG_API os_get_file_mapping_file_handle(file_mapping mapping);

ufprog_status UFPROG_API os_read_text_file(const char *filename, char **outdata, size_t *retlen);

EXTERN_C_END

#endif /* _UFPROG_OSDEF_H_ */
