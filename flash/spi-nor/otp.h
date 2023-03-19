/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash OTP operations
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_OTP_H_
#define _UFPROG_SPI_NOR_OTP_H_

#include "part.h"

uint32_t default_secr_otp_addr(struct spi_nor *snor, uint32_t index, uint32_t addr);
ufprog_status default_secr_otp_lock_bit(struct spi_nor *snor, uint32_t index, uint32_t *retbit,
					const struct spi_nor_reg_access **retacc);

ufprog_status secr_otp_read_naddr(struct spi_nor *snor, uint8_t opcode, uint32_t index, uint32_t addr, uint8_t naddr,
				  uint32_t len, void *data);
ufprog_status secr_otp_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data);
ufprog_status secr_otp_read_paged_naddr(struct spi_nor *snor, uint8_t opcode, uint32_t index, uint32_t addr,
					uint8_t naddr, uint32_t len, void *data);
ufprog_status secr_otp_read_paged(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data);
ufprog_status secr_otp_write_naddr(struct spi_nor *snor, uint8_t opcode, uint32_t index, uint32_t addr, uint8_t naddr,
				   uint32_t len, const void *data);
ufprog_status secr_otp_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data);
ufprog_status secr_otp_write_paged_naddr(struct spi_nor *snor, uint8_t opcode, uint32_t index, uint32_t addr,
					 uint8_t naddr, uint32_t len, const void *data);
ufprog_status secr_otp_write_paged(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data);
ufprog_status secr_otp_erase_naddr(struct spi_nor *snor, uint8_t opcode, uint32_t index, uint8_t naddr);
ufprog_status secr_otp_erase(struct spi_nor *snor, uint32_t index);
ufprog_status secr_otp_lock(struct spi_nor *snor, uint32_t index);
ufprog_status secr_otp_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked);

extern const struct spi_nor_flash_part_otp_ops secr_otp_ops;

ufprog_status scur_otp_read_raw(struct spi_nor *snor, uint64_t addr, size_t len, void *data);
ufprog_status scur_otp_write_raw(struct spi_nor *snor, uint64_t addr, size_t len, const void *data);

ufprog_status scur_otp_read_cust(struct spi_nor *snor, uint32_t addr, uint32_t len, void *data, bool no_exso);
ufprog_status scur_otp_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data);
ufprog_status scur_otp_write_cust(struct spi_nor *snor, uint32_t addr, uint32_t len, const void *data, bool no_exso);
ufprog_status scur_otp_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data);
ufprog_status scur_otp_lock_cust(struct spi_nor *snor, bool no_exso);
ufprog_status scur_otp_lock(struct spi_nor *snor, uint32_t index);
ufprog_status scur_otp_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked);

extern const struct spi_nor_flash_part_otp_ops scur_otp_ops;

#endif /* _UFPROG_SPI_NOR_OTP_H_ */
