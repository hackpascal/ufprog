/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * NAND core module initialization
 */
#pragma once

#ifndef _UFPROG_NAND_CORE_INIT_H_
#define _UFPROG_NAND_CORE_INIT_H_

int ecc_driver_mgmt_init(void);
void ecc_driver_mgmt_deinit(void);

int bbt_driver_mgmt_init(void);
void bbt_driver_mgmt_deinit(void);

int ftl_driver_mgmt_init(void);
void ftl_driver_mgmt_deinit(void);

#endif /* _UFPROG_NAND_CORE_INIT_H_ */
