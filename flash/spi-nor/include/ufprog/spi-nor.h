/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash support
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_H_
#define _UFPROG_SPI_NOR_H_

#include <stdbool.h>
#include <ufprog/device.h>
#include <ufprog/api_spi.h>
#include <ufprog/spi.h>

EXTERN_C_BEGIN

#define SPI_NOR_DFL_ID_LEN			3
#define SPI_NOR_MAX_ID_LEN			6
#define SPI_NOR_MAX_ERASE_INFO			4

/* Status Register 1 fields */
#define SR_BUSY					BIT(0)
#define SR_WEL					BIT(1)
#define SR_BP0					BIT(2)
#define SR_BP1					BIT(3)
#define SR_BP2					BIT(4)

/* Bank Register fields */
#define BANK_4B_ADDR				BIT(7)
#define BANK_SEL_S				0
#define BANK_SEL_M				BITS(6, BANK_SEL_S)

/* Non-volatile Configuration Register fields */
#define NVCR_3B_ADDR				BIT(0)

struct spi_nor;
struct spi_nor_reg_access;

struct spi_nor_id {
	uint8_t id[SPI_NOR_MAX_ID_LEN];
	uint32_t len;
};

struct spi_nor_otp_info {
	uint32_t start_index;
	uint32_t count;
	uint32_t size;
};

struct spi_nor_reg_field_value_item {
	uint32_t value;
	const char *name;
};

struct spi_nor_reg_field_values {
	uint32_t num;

	struct spi_nor_reg_field_value_item items[];
};

struct spi_nor_reg_field_item {
	const char *name;
	const char *desc;
	uint32_t shift;
	uint32_t mask;

	const struct spi_nor_reg_field_values *values;
};

struct spi_nor_reg_def {
	const char *name;
	const char *desc;
	const struct spi_nor_reg_access *access;

	uint8_t nfields;
	const struct spi_nor_reg_field_item *fields;
};

struct snor_reg_info {
	uint8_t num;
	const struct spi_nor_reg_def *regs[];
};

struct spi_nor_wp_region {
	uint64_t base;
	uint64_t size;
};

struct spi_nor_wp_regions {
	uint32_t num;
	const struct spi_nor_wp_region *region;
};

struct spi_nor_erase_region {
	uint64_t size;
	uint32_t min_erasesize;
	uint32_t max_erasesize;
	uint32_t erasesizes_mask;
};

struct spi_nor_info {
	uint32_t signature;

	const char *model;
	const char *vendor;
	struct spi_nor_id id;
	uint64_t size;
	uint32_t ndies;
	uint32_t page_size;
	uint32_t max_speed;
	uint32_t read_io_info;
	uint32_t pp_io_info;
	uint8_t cmd_bw;
	ufprog_bool otp_erasable;

	const void *sfdp_data;
	uint32_t sfdp_size;

	const struct spi_nor_erase_region *erase_regions;
	uint32_t num_erase_regions;

	uint32_t erasesizes[SPI_NOR_MAX_ERASE_INFO];

	const struct spi_nor_otp_info *otp;
	const struct snor_reg_info *regs;
};

struct spi_nor_vendor_item {
	const char *id;
	const char *name;
};

struct spi_nor_probe_part {
	const char *name;
	const char *vendor;
};

struct spi_nor_part_list {
	uint32_t num;
	struct spi_nor_probe_part list[];
};

ufprog_status UFPROG_API ufprog_spi_nor_load_ext_id_file(void);

struct spi_nor *UFPROG_API ufprog_spi_nor_create(void);
ufprog_status UFPROG_API ufprog_spi_nor_destroy(struct spi_nor *snor);

ufprog_status UFPROG_API ufprog_spi_nor_attach(struct spi_nor *snor, struct ufprog_spi *spi);
ufprog_status UFPROG_API ufprog_spi_nor_detach(struct spi_nor *snor, ufprog_bool close_if);
struct ufprog_spi *UFPROG_API ufprog_spi_nor_get_interface_device(struct spi_nor *snor);

ufprog_status UFPROG_API ufprog_spi_nor_bus_lock(struct spi_nor *snor);
ufprog_status UFPROG_API ufprog_spi_nor_bus_unlock(struct spi_nor *snor);

uint32_t UFPROG_API ufprog_spi_nor_get_allowed_io_caps(struct spi_nor *snor);
void UFPROG_API ufprog_spi_nor_set_allowed_io_caps(struct spi_nor *snor, uint32_t io_caps);

uint32_t UFPROG_API ufprog_spi_nor_get_speed_limit(struct spi_nor *snor);
void UFPROG_API ufprog_spi_nor_set_speed_limit(struct spi_nor *snor, uint32_t hz);
uint32_t UFPROG_API ufprog_spi_nor_get_speed_low(struct spi_nor *snor);
uint32_t UFPROG_API ufprog_spi_nor_get_speed_high(struct spi_nor *snor);

ufprog_status UFPROG_API ufprog_spi_nor_list_vendors(struct spi_nor_vendor_item **outlist, uint32_t *retcount);

ufprog_status UFPROG_API ufprog_spi_nor_list_parts(struct spi_nor_part_list **outlist, const char *vendorid,
						   const char *match);

ufprog_status UFPROG_API ufprog_spi_nor_probe(struct spi_nor *snor, struct spi_nor_part_list **outlist,
					      struct spi_nor_id *retid);
ufprog_status UFPROG_API ufprog_spi_nor_free_list(void *list);

ufprog_status UFPROG_API ufprog_spi_nor_part_init(struct spi_nor *snor, const char *vendorid, const char *part,
						  ufprog_bool forced_init);
ufprog_status UFPROG_API ufprog_spi_nor_probe_init(struct spi_nor *snor);

ufprog_bool UFPROG_API ufprog_spi_nor_valid(struct spi_nor *snor);
uint32_t UFPROG_API ufprog_spi_nor_flash_param_signature(struct spi_nor *snor);
ufprog_status UFPROG_API ufprog_spi_nor_info(struct spi_nor *snor, struct spi_nor_info *info);

ufprog_status UFPROG_API ufprog_spi_nor_select_die(struct spi_nor *snor, uint32_t index);

ufprog_status UFPROG_API ufprog_spi_nor_set_bus_width(struct spi_nor *snor, uint8_t buswidth);

ufprog_status UFPROG_API ufprog_spi_nor_read_no_check(struct spi_nor *snor, uint64_t addr, size_t len, void *data);
ufprog_status UFPROG_API ufprog_spi_nor_read(struct spi_nor *snor, uint64_t addr, size_t len, void *buf);

ufprog_status UFPROG_API ufprog_spi_nor_write_page_no_check(struct spi_nor *snor, uint64_t addr, size_t len,
							    const void *data, size_t *retlen);
ufprog_status UFPROG_API ufprog_spi_nor_write_page(struct spi_nor *snor, uint64_t addr, size_t len, const void *data,
						   size_t *retlen);
ufprog_status UFPROG_API ufprog_spi_nor_write(struct spi_nor *snor, uint64_t addr, size_t len, const void *data);

const struct spi_nor_erase_region *UFPROG_API ufprog_spi_nor_get_erase_region_at(struct spi_nor *snor, uint64_t addr);
ufprog_status UFPROG_API ufprog_spi_nor_get_erase_range(struct spi_nor *snor, uint64_t addr, uint64_t len,
							uint64_t *retaddr_start, uint64_t *retaddr_end);
ufprog_status UFPROG_API ufprog_spi_nor_erase_at(struct spi_nor *snor, uint64_t addr, uint64_t maxlen,
						 uint32_t *ret_eraseszie);
ufprog_status UFPROG_API ufprog_spi_nor_erase(struct spi_nor *snor, uint64_t addr, uint64_t len);

ufprog_status UFPROG_API ufprog_spi_nor_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen);

uint32_t UFPROG_API ufprog_spi_nor_get_reg_bytes(const struct spi_nor_reg_access *access);
ufprog_status UFPROG_API ufprog_spi_nor_read_reg(struct spi_nor *snor, const struct spi_nor_reg_access *access,
						 uint32_t *retval);
ufprog_status UFPROG_API ufprog_spi_nor_write_reg(struct spi_nor *snor, const struct spi_nor_reg_access *access,
						  uint32_t retval);
ufprog_status UFPROG_API ufprog_spi_nor_update_reg(struct spi_nor *snor, const struct spi_nor_reg_access *access,
						   uint32_t clr, uint32_t set);

ufprog_status UFPROG_API ufprog_spi_nor_otp_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
						 void *data);
ufprog_status UFPROG_API ufprog_spi_nor_otp_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
						  const void *data);
ufprog_status UFPROG_API ufprog_spi_nor_otp_erase(struct spi_nor *snor, uint32_t index);
ufprog_status UFPROG_API ufprog_spi_nor_otp_lock(struct spi_nor *snor, uint32_t index);
ufprog_status UFPROG_API ufprog_spi_nor_otp_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked);

ufprog_status UFPROG_API ufprog_spi_nor_get_wp_region_list(struct spi_nor *snor, struct spi_nor_wp_regions *retregions);
ufprog_status UFPROG_API ufprog_spi_nor_get_wp_region(struct spi_nor *snor, struct spi_nor_wp_region *retregion);
ufprog_status UFPROG_API ufprog_spi_nor_set_wp_region(struct spi_nor *snor, const struct spi_nor_wp_region *region);

EXTERN_C_END

#endif /* _UFPROG_SPI_NOR_H_ */
