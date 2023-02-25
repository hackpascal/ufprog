/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Device module initialization
 */
#pragma once

#ifndef _UFPROG_DEVICE_INIT_H_
#define _UFPROG_DEVICE_INIT_H_

int driver_lookup_table_init(void);
int libusb_global_init(void);

#endif /* _UFPROG_DEVICE_INIT_H_ */
