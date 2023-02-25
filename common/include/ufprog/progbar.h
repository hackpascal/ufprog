/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Simple console progress bar
 */
#pragma once

#ifndef _UFPROG_PROGBAR_H_
#define _UFPROG_PROGBAR_H_

#include <stdint.h>
#include <ufprog/common.h>

EXTERN_C_BEGIN

void UFPROG_API progress_init(void);
void UFPROG_API progress_show(uint32_t percentage);
void UFPROG_API progress_done(void);

EXTERN_C_END

#endif /* _UFPROG_PROGBAR_H_ */
