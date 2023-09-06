/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * RAM-based BBT implementation
 */
#pragma once

#ifndef _UFPROG_BBT_RAM_H_
#define _UFPROG_BBT_RAM_H_

#include <ufprog/bbt.h>

EXTERN_C_BEGIN

ufprog_status UFPROG_API ufprog_bbt_ram_create(const char *name, struct nand_chip *nand,
					       struct ufprog_nand_bbt **outbbt);

EXTERN_C_END

#endif /* _UFPROG_BBT_H_ */
