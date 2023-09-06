/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Builtin FTL implementation
 */
#pragma once

#ifndef _UFPROG_FTL_BASIC_H_
#define _UFPROG_FTL_BASIC_H_

#include <ufprog/ftl.h>
#include <ufprog/bbt.h>

EXTERN_C_BEGIN

#define FTL_BASIC_F_DONT_CHECK_BAD			BIT(0)

ufprog_status UFPROG_API ufprog_ftl_basic_create(const char *name, struct nand_chip *nand, struct ufprog_nand_bbt *bbt,
						 uint32_t flags, struct ufprog_nand_ftl **outftl);

EXTERN_C_END

#endif /* _UFPROG_FTL_BASIC_H_ */
