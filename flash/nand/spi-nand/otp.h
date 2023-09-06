/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash OTP page operations
 */
#pragma once

#ifndef _UFPROG_SPI_NAND_OTP_H_
#define _UFPROG_SPI_NAND_OTP_H_

ufprog_status spi_nand_otp_read(struct nand_chip *nand, uint32_t index, uint32_t column, uint32_t size, void *data);
ufprog_status spi_nand_otp_write(struct nand_chip *nand, uint32_t index, uint32_t column, uint32_t size,
				 const void *data);
ufprog_status spi_nand_otp_lock(struct nand_chip *nand);
ufprog_status spi_nand_otp_locked(struct nand_chip *nand, ufprog_bool *retlocked);

ufprog_status spi_nand_otp_micron_lock(struct nand_chip *nand);
ufprog_status spi_nand_otp_micron_locked(struct nand_chip *nand, ufprog_bool *retlocked);

extern const struct nand_flash_otp_ops spi_nand_otp_ops;
extern const struct nand_flash_otp_ops spi_nand_otp_micron_ops;

#endif /* _UFPROG_SPI_NAND_OTP_H_ */
