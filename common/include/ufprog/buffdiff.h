/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Generic memory buffer comparision implementation
 */
#pragma once

#ifndef _UFPROG_BUFFDIFF_H_
#define _UFPROG_BUFFDIFF_H_

#include <ufprog/common.h>
#include <ufprog/osdef.h>

EXTERN_C_BEGIN

ufprog_bool UFPROG_API bufdiff(const void *a, const void *b, size_t len, size_t *retpos);

EXTERN_C_END

#endif /* _UFPROG_BUFFDIFF_H_ */
