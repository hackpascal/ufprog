/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Etron SPI-NAND flash definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NAND_VENDOR_ETRON_H_
#define _UFPROG_SPI_NAND_VENDOR_ETRON_H_

#include "core.h"

extern const struct nand_page_layout etron_2k_64_ecc_layout;
extern const struct nand_page_layout etron_2k_128_ecc_layout;
extern const struct nand_page_layout etron_4k_256_ecc_layout;

#endif /* _UFPROG_SPI_NAND_VENDOR_ETRON_H_ */
