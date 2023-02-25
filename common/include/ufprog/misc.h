/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Misc helpers
 */
#pragma once

#ifndef _UFPROG_MISC_H_
#define _UFPROG_MISC_H_

#include <ufprog/common.h>

EXTERN_C_BEGIN

char *UFPROG_API bin_to_hex_str(char *buf, size_t bufsize, const void *data, size_t datasize, ufprog_bool space,
				ufprog_bool uppercase);

ufprog_status UFPROG_API read_file_contents(const char *filename, void **outdata, size_t *retsize);
ufprog_status UFPROG_API write_file_contents(const char *filename, const void *data, size_t len, ufprog_bool create);

EXTERN_C_END

#endif /* _UFPROG_MISC_H_ */
