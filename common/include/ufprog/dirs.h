/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Directories for configs/drivers
 */
#pragma once

#ifndef _UFPROG_DIRS_H_
#define _UFPROG_DIRS_H_

#include <stdint.h>
#include <stdbool.h>
#include <ufprog/osdef.h>

EXTERN_C_BEGIN

enum dir_category {
	DIR_DATA_ROOT,
	DIR_CONFIG,
	DIR_DEVICE,
	DIR_PLUGIN,

	__MAX_DIR_CATEGORY
};

typedef int (UFPROG_API *dir_enum_cb)(void *priv, uint32_t index, const char *dir);

ufprog_bool UFPROG_API uses_portable_dirs(void);

ufprog_status UFPROG_API set_root_dir(const char *dir);
const char *UFPROG_API get_root_dir(void);

/* Directory being added must end with path separator */
ufprog_status UFPROG_API add_dir(enum dir_category cat, const char *dir);
const char *UFPROG_API get_dir(enum dir_category cat, uint32_t index);
void UFPROG_API dir_enum(enum dir_category cat, dir_enum_cb cb, void *priv);

char *UFPROG_API path_concat(ufprog_bool end_sep, size_t extra_len, const char *base, ...);

EXTERN_C_END

#endif /* _UFPROG_DIRS_H_ */
