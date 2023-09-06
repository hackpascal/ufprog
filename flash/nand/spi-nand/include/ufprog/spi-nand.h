/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash support
 */
#pragma once

#ifndef _UFPROG_SPI_NAND_H_
#define _UFPROG_SPI_NAND_H_

#include <stdbool.h>
#include <ufprog/device.h>
#include <ufprog/api_spi.h>
#include <ufprog/spi.h>
#include <ufprog/nand.h>

EXTERN_C_BEGIN

#define SPI_NAND_ID_LEN				3

/* SPI-NAND core configuration */
#define SPI_NAND_CFG_DIRECT_MULTI_PAGE_READ	BIT(0)

/* SPI-NAND feature addresses */
#define SPI_NAND_FEATURE_BFR7_0_ADDR		0x40
#define SPI_NAND_FEATURE_BFR15_8_ADDR		0x50
#define SPI_NAND_FEATURE_BFR23_16_ADDR		0x60
#define SPI_NAND_FEATURE_BFR31_24_ADDR		0x70

#define SPI_NAND_FEATURE_STATUS_ADDR		0xc0
#define SPI_NAND_STATUS_OIP			BIT(0)
#define SPI_NAND_STATUS_WEL			BIT(1)
#define SPI_NAND_STATUS_ERASE_FAIL		BIT(2)
#define SPI_NAND_STATUS_PROGRAM_FAIL		BIT(3)
#define SPI_NAND_STATUS_ECC_SHIFT		4
#define SPI_NAND_STATUS_ECC_MASK		BITS(5, SPI_NAND_STATUS_ECC_SHIFT)
#define SPI_NAND_STATUS_CRBSY			BIT(7)

#define SPI_NAND_FEATURE_CONFIG_ADDR		0xb0
#define SPI_NAND_CONFIG_QUAD_EN			BIT(0)
#define SPI_NAND_CONFIG_ECC_EN			BIT(4)
#define SPI_NAND_CONFIG_OTP_EN			BIT(6)
#define SPI_NAND_CONFIG_OTP_LOCK		BIT(7)

#define SPI_NAND_FEATURE_PROTECT_ADDR		0xa0

#define SPI_NAND_ONFI_ALT_SIGNATURE		0x444E414E

struct spi_nand;

struct spi_nand_info {
	uint32_t signature;

	uint32_t max_speed;
	uint32_t rd_io_info;
	uint32_t pl_io_info;

	const void *onfi_data;
};

struct spi_nand_vendor_item {
	const char *id;
	const char *name;
};

struct spi_nand_probe_part {
	const char *name;
	const char *vendor;
};

struct spi_nand_part_list {
	uint32_t num;
	struct spi_nand_probe_part list[];
};

ufprog_status UFPROG_API ufprog_spi_nand_load_ext_id_file(void);

struct spi_nand *UFPROG_API ufprog_spi_nand_create(void);
ufprog_status UFPROG_API ufprog_spi_nand_destroy(struct spi_nand *snand);

ufprog_status UFPROG_API ufprog_spi_nand_attach(struct spi_nand *snand, struct ufprog_spi *spi);
ufprog_status UFPROG_API ufprog_spi_nand_detach(struct spi_nand *snand, ufprog_bool close_if);
struct ufprog_spi *UFPROG_API ufprog_spi_nand_get_interface_device(struct spi_nand *snand);
struct nand_chip *UFPROG_API ufprog_spi_nand_get_generic_nand_interface(struct spi_nand *snand);

ufprog_status UFPROG_API ufprog_spi_nand_bus_lock(struct spi_nand *snand);
ufprog_status UFPROG_API ufprog_spi_nand_bus_unlock(struct spi_nand *snand);

uint32_t UFPROG_API ufprog_spi_nand_get_allowed_io_caps(struct spi_nand *snand);
void UFPROG_API ufprog_spi_nand_set_allowed_io_caps(struct spi_nand *snand, uint32_t io_caps);

uint32_t UFPROG_API ufprog_spi_nand_get_config(struct spi_nand *snand);
void UFPROG_API ufprog_spi_nand_modify_config(struct spi_nand *snand, uint32_t clr, uint32_t set);

uint32_t UFPROG_API ufprog_spi_nand_get_speed_limit(struct spi_nand *snand);
void UFPROG_API ufprog_spi_nand_set_speed_limit(struct spi_nand *snand, uint32_t hz);
uint32_t UFPROG_API ufprog_spi_nand_get_speed_low(struct spi_nand *snand);
uint32_t UFPROG_API ufprog_spi_nand_get_speed_high(struct spi_nand *snand);

ufprog_status UFPROG_API ufprog_spi_nand_list_vendors(struct spi_nand_vendor_item **outlist, uint32_t *retcount);

ufprog_status UFPROG_API ufprog_spi_nand_list_parts(struct spi_nand_part_list **outlist, const char *vendorid,
						    const char *match);

ufprog_status UFPROG_API ufprog_spi_nand_probe(struct spi_nand *snand, struct spi_nand_part_list **outlist,
					       struct nand_id *retid);
ufprog_status UFPROG_API ufprog_spi_nand_free_list(void *list);

ufprog_status UFPROG_API ufprog_spi_nand_part_init(struct spi_nand *snand, const char *vendorid, const char *part);
ufprog_status UFPROG_API ufprog_spi_nand_probe_init(struct spi_nand *snand);

ufprog_bool UFPROG_API ufprog_spi_nand_valid(struct spi_nand *snand);
uint32_t UFPROG_API ufprog_spi_nand_flash_param_signature(struct spi_nand *snand);
ufprog_status UFPROG_API ufprog_spi_nand_info(struct spi_nand *snand, struct spi_nand_info *info);

ufprog_bool UFPROG_API ufprog_spi_nand_supports_nor_read(struct spi_nand *snand);
ufprog_status UFPROG_API ufprog_spi_nand_enable_nor_read(struct spi_nand *snand);
ufprog_status UFPROG_API ufprog_spi_nand_nor_read_enabled(struct spi_nand *snand, ufprog_bool *retenabled);

EXTERN_C_END

#endif /* _UFPROG_SPI_NAND_H_ */
