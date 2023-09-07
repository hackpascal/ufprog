// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash core
 */

#include <malloc.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ufprog/log.h>
#include <ufprog/misc.h>
#include <ufprog/sizes.h>
#include <ufprog/crc32.h>
#include "core.h"
#include "ecc.h"
#include "ext_id.h"

#define ecc_to_spi_nand(_ecc)	container_of(_ecc, struct spi_nand, ecc)

static void spi_nand_reset_param(struct spi_nand *snand);
static inline ufprog_status spi_nand_op_read_page_to_cache(struct spi_nand *snand, uint32_t page);

static ufprog_status spi_nand_ops_read_page(struct nand_chip *nand, uint32_t page, uint32_t column, uint32_t len,
					    void *buf);
static ufprog_status spi_nand_ops_read_pages(struct nand_chip *nand, uint32_t page, uint32_t count, void *buf,
					     uint32_t flags, uint32_t *retcount);
static ufprog_status spi_nand_ops_write_page(struct nand_chip *nand, uint32_t page, uint32_t column, uint32_t len,
					     const void *buf);
static ufprog_status spi_nand_ops_erase_block(struct nand_chip *nand, uint32_t page);
static ufprog_status spi_nand_ops_select_die(struct nand_chip *nand, uint32_t ce, uint32_t lun);
static ufprog_status spi_nand_ops_read_uid(struct nand_chip *nand, void *data, uint32_t *retlen);

static ufprog_status spi_nand_read_uid_generic(struct spi_nand *snand, void *data, uint32_t *retlen);

ufprog_status UFPROG_API ufprog_spi_nand_load_ext_id_file(void)
{
	return spi_nand_load_ext_id_list();
}

struct spi_nand *UFPROG_API ufprog_spi_nand_create(void)
{
	struct spi_nand *snand;

	snand = calloc(1, sizeof(*snand));
	if (!snand) {
		logm_err("No memory for SPI-NOR object\n");
		return NULL;
	}

	snand->max_speed = SNAND_SPEED_HIGH;
	snand->allowed_io_caps = (1 << __SPI_MEM_IO_MAX) - 1;

	return snand;
}

ufprog_status UFPROG_API ufprog_spi_nand_destroy(struct spi_nand *snand)
{
	if (!snand)
		return UFP_INVALID_PARAMETER;

	spi_nand_reset_param(snand);
	free(snand);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nand_attach(struct spi_nand *snand, struct ufprog_spi *spi)
{
	if (!snand || !spi)
		return UFP_INVALID_PARAMETER;

	if (snand->spi) {
		logm_err("The SPI-NAND object has already attached to a SPI interface device\n");
		return UFP_FAIL;
	}

	snand->spi = spi;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nand_detach(struct spi_nand *snand, ufprog_bool close_if)
{
	if (!snand)
		return UFP_INVALID_PARAMETER;

	spi_nand_reset_param(snand);

	if (!snand->spi)
		return UFP_OK;

	if (close_if)
		ufprog_spi_close_device(snand->spi);

	snand->spi = NULL;

	return UFP_OK;
}

struct ufprog_spi *UFPROG_API ufprog_spi_nand_get_interface_device(struct spi_nand *snand)
{
	if (!snand)
		return NULL;

	return snand->spi;
}

struct nand_chip *UFPROG_API ufprog_spi_nand_get_generic_nand_interface(struct spi_nand *snand)
{
	if (!snand)
		return NULL;

	return &snand->nand;
}

ufprog_status UFPROG_API ufprog_spi_nand_bus_lock(struct spi_nand *snand)
{
	return ufprog_spi_bus_lock(snand->spi);
}

ufprog_status UFPROG_API ufprog_spi_nand_bus_unlock(struct spi_nand *snand)
{
	return ufprog_spi_bus_unlock(snand->spi);
}

uint32_t UFPROG_API ufprog_spi_nand_get_allowed_io_caps(struct spi_nand *snand)
{
	if (!snand)
		return 0;

	return snand->allowed_io_caps;
}

void UFPROG_API ufprog_spi_nand_set_allowed_io_caps(struct spi_nand *snand, uint32_t io_caps)
{
	if (!snand)
		return;

	snand->allowed_io_caps = io_caps;
}

uint32_t UFPROG_API ufprog_spi_nand_get_config(struct spi_nand *snand)
{
	if (!snand)
		return 0;

	return snand->config;
}

void UFPROG_API ufprog_spi_nand_modify_config(struct spi_nand *snand, uint32_t clr, uint32_t set)
{
	if (!snand)
		return;

	snand->config &= ~clr;
	snand->config |= set;
}

uint32_t UFPROG_API ufprog_spi_nand_get_speed_limit(struct spi_nand *snand)
{
	if (!snand)
		return 0;

	return snand->max_speed;
}

void UFPROG_API ufprog_spi_nand_set_speed_limit(struct spi_nand *snand, uint32_t hz)
{
	if (!snand)
		return;

	snand->max_speed = hz;
}

uint32_t UFPROG_API ufprog_spi_nand_get_speed_low(struct spi_nand *snand)
{
	if (!snand)
		return 0;

	return snand->state.speed_low;
}

uint32_t UFPROG_API ufprog_spi_nand_get_speed_high(struct spi_nand *snand)
{
	if (!snand)
		return 0;

	return snand->state.speed_high;
}

static ufprog_status spi_nand_set_speed(struct spi_nand *snand, uint32_t speed)
{
	ufprog_status ret;

	ret = ufprog_spi_set_speed(snand->spi, speed, NULL);
	if (!ret || ret == UFP_UNSUPPORTED)
		return UFP_OK;

	return ret;
}

ufprog_status spi_nand_set_low_speed(struct spi_nand *snand)
{
	return spi_nand_set_speed(snand, snand->state.speed_low);
}

ufprog_status spi_nand_set_high_speed(struct spi_nand *snand)
{
	return spi_nand_set_speed(snand, snand->state.speed_high);
}

ufprog_status spi_nand_issue_single_opcode(struct spi_nand *snand, uint8_t opcode)
{
	struct ufprog_spi_mem_op op = SNAND_SINGLE_OP(opcode);

	return ufprog_spi_mem_exec_op(snand->spi, &op);
}

ufprog_status spi_nand_get_feature(struct spi_nand *snand, uint32_t addr, uint8_t *retval)
{
	struct ufprog_spi_mem_op op = SNAND_GET_FEATURE_OP(addr, retval);

	return ufprog_spi_mem_exec_op(snand->spi, &op);
}

ufprog_status spi_nand_set_feature(struct spi_nand *snand, uint32_t addr, uint8_t val)
{
	struct ufprog_spi_mem_op op = SNAND_SET_FEATURE_OP(addr, &val);

	return ufprog_spi_mem_exec_op(snand->spi, &op);
}

static ufprog_status spi_nand_read_id(struct spi_nand *snand, uint8_t *id, uint32_t len)
{
	struct ufprog_spi_mem_op op = SNAND_READ_ID_OP(len, 1, id);

	return ufprog_spi_mem_exec_op(snand->spi, &op);
}

static ufprog_status spi_nand_read_id_direct(struct spi_nand *snand, uint8_t *id, uint32_t len)
{
	struct ufprog_spi_mem_op op = SNAND_READ_ID_OP(len, 0, id);

	return ufprog_spi_mem_exec_op(snand->spi, &op);
}

static ufprog_status spi_nand_read_id_addr(struct spi_nand *snand, uint8_t *id, uint32_t len, uint8_t addr)
{
	struct ufprog_spi_mem_op op = SNAND_READ_ID_ADDR_OP(len, addr, id);

	return ufprog_spi_mem_exec_op(snand->spi, &op);
}

ufprog_status spi_nand_read_status(struct spi_nand *snand, uint8_t *retval)
{
	ufprog_status ret;

	ret = spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, retval);
	if (ret)
		logm_err("Failed to read status register\n");

	return ret;
}

static ufprog_status spi_nand_wait_busy_bit(struct spi_nand *snand, uint8_t addr, uint8_t bitm, uint32_t wait_us,
					    uint8_t *retsr)
{
	uint64_t tst = os_get_timer_us(), tmo = tst + wait_us;
	ufprog_status ret;
	uint8_t sr = 0;

	do {
		ret = spi_nand_get_feature(snand, addr, &sr);
		if (ret) {
			logm_err("Failed to read feature address 0x%02x\n", addr);
			return ret;
		}

		if (!(sr & bitm))
			break;
	} while (os_get_timer_us() <= tmo);

	/* Last check */
	if (sr & bitm) {
		ret = spi_nand_get_feature(snand, addr, &sr);
		if (ret) {
			logm_err("Failed to read feature address 0x%02x\n", addr);
			return ret;
		}
	}

	if (retsr)
		*retsr = sr;

	if (!(sr & bitm))
		return UFP_OK;

	return UFP_TIMEOUT;
}

ufprog_status spi_nand_wait_busy(struct spi_nand *snand, uint32_t wait_us, uint8_t *retsr)
{
	ufprog_status ret;

	ret = spi_nand_wait_busy_bit(snand, SPI_NAND_FEATURE_STATUS_ADDR, SPI_NAND_STATUS_OIP, wait_us, retsr);
	if (ret)
		logm_err("Timed out waiting for flash idle\n");

	return ret;
}

static ufprog_status spi_nand_refresh_config(struct spi_nand *snand)
{
	ufprog_status ret;
	uint8_t cr;

	ret = spi_nand_get_feature(snand, SPI_NAND_FEATURE_CONFIG_ADDR, &cr);
	if (ret) {
		logm_err("Failed to read configuration register\n");
		return ret;
	}

	snand->state.cfg[snand->state.curr_die] = cr;

	return UFP_OK;
}

uint8_t spi_nand_get_config(struct spi_nand *snand)
{
	return snand->state.cfg[snand->state.curr_die];
}

static ufprog_status spi_nand_set_config(struct spi_nand *snand, uint8_t val)
{
	ufprog_status ret;

	if (val == snand->state.cfg[snand->state.curr_die])
		return UFP_OK;

	ret = spi_nand_set_feature(snand, SPI_NAND_FEATURE_CONFIG_ADDR, val);
	if (ret) {
		logm_err("Failed to write configuration register\n");
		return ret;
	}

	snand->state.cfg[snand->state.curr_die] = val;

	return UFP_OK;
}

ufprog_status spi_nand_update_config(struct spi_nand *snand, uint8_t clr, uint8_t set)
{
	uint8_t cr, ncr;

	cr = spi_nand_get_config(snand);
	ncr = cr & ~clr;
	ncr |= set;

	if (cr == ncr)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nand_set_config(snand, ncr));
	STATUS_CHECK_RET(spi_nand_refresh_config(snand));

	if (spi_nand_get_config(snand) != ncr) {
		logm_warn("Failed to update configuration register bits\n");
		return UFP_UNSUPPORTED;
	}

	return UFP_OK;
}

static ufprog_status spi_nand_chip_reset(struct spi_nand *snand)
{
	ufprog_status ret;

	STATUS_CHECK_RET(spi_nand_issue_single_opcode(snand, SNAND_CMD_RESET));

	ret = spi_nand_wait_busy(snand, SNAND_RESET_WAIT_US, NULL);
	if (ret)
		logm_err("Failed to wait flash ready after reset\n");

	return ret;
}

static ufprog_status spi_nand_unlock(struct spi_nand *snand)
{
	ufprog_status ret;

	ret = spi_nand_set_feature(snand, SPI_NAND_FEATURE_PROTECT_ADDR, 0);
	if (ret)
		logm_err("Failed to write configuration register\n");

	return ret;
}

ufprog_status spi_nand_write_enable(struct spi_nand *snand)
{
	uint8_t sr;

	STATUS_CHECK_RET(spi_nand_issue_single_opcode(snand, SNAND_CMD_WRITE_ENABLE));
	STATUS_CHECK_RET(spi_nand_read_status(snand, &sr));

	if (sr & SPI_NAND_STATUS_WEL)
		return UFP_OK;

	logm_err("Failed to issue write-enable command\n");

	return UFP_FAIL;
}

ufprog_status spi_nand_write_disable(struct spi_nand *snand)
{
	uint8_t sr;

	STATUS_CHECK_RET(spi_nand_issue_single_opcode(snand, SNAND_CMD_WRITE_DISABLE));
	STATUS_CHECK_RET(spi_nand_read_status(snand, &sr));

	if (!(sr & SPI_NAND_STATUS_WEL))
		return UFP_OK;

	logm_err("Failed to issue write-disable command\n");

	return UFP_FAIL;
}

static ufprog_status spi_nand_select_die(struct spi_nand *snand, uint32_t dieidx)
{
	ufprog_status ret;

	if (snand->state.curr_die == dieidx)
		return UFP_OK;

	if (snand->ext_param.ops.select_die) {
		ret = snand->ext_param.ops.select_die(snand, dieidx);
		if (ret) {
			logm_err("Failed to select die #%u\n", dieidx);
			return ret;
		}
	}

	snand->state.curr_die = (uint8_t)dieidx;

	return UFP_OK;
}

static ufprog_status spi_nand_select_die_page(struct spi_nand *snand, uint32_t *page)
{
	struct nand_chip *nand = &snand->nand;
	uint32_t dieidx;

	if (!snand->ext_param.ops.select_die)
		return UFP_OK;

	dieidx = *page >> (nand->maux.lun_shift - nand->maux.page_shift);

	STATUS_CHECK_RET(spi_nand_select_die(snand, dieidx));

	*page &= nand->maux.lun_mask >> nand->maux.page_shift;

	return UFP_OK;
}

static inline uint32_t spi_nand_get_plane_address(struct spi_nand *snand, uint32_t page)
{
	struct nand_chip *nand = &snand->nand;

	if (page & nand->memorg.pages_per_block)
		return 1 << (nand->maux.page_shift + 1);

	return 0;
}

ufprog_status spi_nand_select_die_c2h(struct spi_nand *snand, uint32_t dieidx)
{
	uint8_t val = (uint8_t)dieidx;
	struct ufprog_spi_mem_op op = SNAND_SELECT_DIE_OP(&val);

	return ufprog_spi_mem_exec_op(snand->spi, &op);
}

static ufprog_status spi_nand_ecc_control_cr_bit4(struct spi_nand *snand, bool enable)
{
	ufprog_status ret;

	if (enable)
		ret = spi_nand_update_config(snand, 0, SPI_NAND_CONFIG_ECC_EN);
	else
		ret = spi_nand_update_config(snand, SPI_NAND_CONFIG_ECC_EN, 0);

	if (ret)
		logm_err("Failed to %s On-Die ECC engine\n", enable ? "enable" : "disable");

	return ret;
}

static ufprog_status spi_nand_ecc_control_always_on(struct spi_nand *snand, bool enable)
{

	if (!enable && !snand->state.ecc_warn_once) {
		logm_warn("The on-die ECC engine cannot be disabled\n");
		snand->state.ecc_warn_once = true;
	}

	return UFP_OK;
}

static ufprog_status spi_nand_quad_enable_cr_bit0(struct spi_nand *snand)
{
	ufprog_status ret;

	ret = spi_nand_update_config(snand, 0, SPI_NAND_CONFIG_QUAD_EN);
	if (ret)
		logm_err("Failed to enable Quad-SPI\n");

	return ret;
}

static ufprog_status spi_nand_otp_control_cr_bit6(struct spi_nand *snand, bool enable)
{
	ufprog_status ret;

	if (enable)
		ret = spi_nand_update_config(snand, 0, SPI_NAND_CONFIG_OTP_EN);
	else
		ret = spi_nand_update_config(snand, SPI_NAND_CONFIG_OTP_EN, 0);

	if (ret)
		logm_err("Failed to %s OTP mode\n", enable ? "enable" : "disable");

	return ret;
}

ufprog_status spi_nand_ondie_ecc_control(struct spi_nand *snand, bool enable)
{
	if (!snand->ext_param.ops.ecc_control)
		return enable ? UFP_UNSUPPORTED : UFP_OK;

	return snand->ext_param.ops.ecc_control(snand, enable);
}

static ufprog_status spi_nand_qspi_enable(struct spi_nand *snand)
{
	if (!snand->ext_param.ops.quad_enable)
		return UFP_OK;

	return snand->ext_param.ops.quad_enable(snand);
}

ufprog_status spi_nand_otp_control(struct spi_nand *snand, bool enable)
{
	if (!snand->ext_param.ops.otp_control)
		return UFP_UNSUPPORTED;

	return snand->ext_param.ops.otp_control(snand, enable);
}

static ufprog_status spi_nand_read_match_jedec_id(struct spi_nand *snand, uint8_t mfr_id, uint8_t *id)
{
	ufprog_status ret;

	ret = spi_nand_read_id(snand, id, SPI_NAND_ID_LEN);
	if (!ret) {
		if (id[0] == mfr_id)
			return UFP_OK;
	}

	ret = spi_nand_read_id_addr(snand, id, SPI_NAND_ID_LEN, 0);
	if (!ret) {
		if (id[0] == mfr_id)
			return UFP_OK;
	}

	ret = spi_nand_read_id_direct(snand, id, SPI_NAND_ID_LEN);
	if (!ret) {
		if (id[0] == mfr_id)
			return UFP_OK;
	}

	return UFP_FAIL;
}

static bool spi_nand_probe_jedec_id_retry(struct spi_nand *snand, struct spi_nand_vendor_part *retvp,
					  enum spi_nand_id_type type, uint32_t retries)
{
	ufprog_status ret;
	bool probed;
	uint32_t i;

	retvp->part = NULL;
	retvp->vendor = NULL;

	for (i = 0; i < retries; i++) {
		switch (type) {
		case SNAND_ID_DUMMY:
			ret = spi_nand_read_id(snand, snand->nand.id.id, SPI_NAND_ID_LEN);
			break;

		case SNAND_ID_ADDR:
			ret = spi_nand_read_id_addr(snand, snand->nand.id.id, SPI_NAND_ID_LEN, 0);
			break;

		case SNAND_ID_DIRECT:
			ret = spi_nand_read_id_direct(snand, snand->nand.id.id, SPI_NAND_ID_LEN);
			break;

		default:
			ret = UFP_UNSUPPORTED;
		}

		if (ret) {
			if (ret == UFP_UNSUPPORTED)
				return false;
			continue;
		}

		probed = spi_nand_find_vendor_part(type, snand->nand.id.id, retvp);
		if (probed) {
			snand->nand.id.len = retvp->part->id.val.len;
			return true;
		}
	}

	return false;
}

static bool spi_nand_probe_jedec_id(struct spi_nand *snand, struct spi_nand_vendor_part *retvp)
{
	enum spi_nand_id_type type;
	char idstr[20];
	bool rc;

	if (spi_nand_set_low_speed(snand))
		logm_warn("Failed to set spi bus low speed\n");

	for (type = 0; type < __SNAND_ID_TYPE_MAX; type++) {
		rc = spi_nand_probe_jedec_id_retry(snand, retvp, type, SNAND_ID_PROBE_RETRIES);
		if (rc) {
			logm_dbg("Matched predefined model: %s\n", retvp->part->model);
			bin_to_hex_str(idstr, sizeof(idstr), retvp->part->id.val.id, retvp->part->id.val.len, true,
				       true);
			logm_dbg("Matched JEDEC ID: %s\n", idstr);

			return true;
		}
	}

	logm_notice("Unable to identify SPI-NAND chip using JEDEC ID\n");

	return false;
}

static void spi_nand_parse_onfi_fill_time(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	snand->param.max_pp_time_us = ufprog_pp_read_u16(snand->onfi.data, ONFI_T_PROG_MAX_OFFS);
	snand->param.max_be_time_us = ufprog_pp_read_u16(snand->onfi.data, ONFI_T_BERS_MAX_OFFS);
	snand->param.max_r_time_us = ufprog_pp_read_u16(snand->onfi.data, ONFI_T_R_MAX_OFFS);
}

static void spi_nand_parse_onfi_fill(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	uint32_t nop;

	ufprog_pp_read_str(snand->onfi.data, bp->vendor, sizeof(bp->vendor), PP_MANUF_OFFS, PP_MANUF_LEN);
	ufprog_pp_read_str(snand->onfi.data, bp->model, sizeof(bp->model), PP_MODEL_OFFS, PP_MODEL_LEN);
	ufprog_pp_resolve_memorg(snand->onfi.data, &bp->memorg);
	spi_nand_parse_onfi_fill_time(snand, bp);

	nop = ufprog_pp_read_u8(snand->onfi.data, ONFI_NUM_PROGS_PER_PAGE_OFFS);
	if (nop)
		bp->p.ecc_req.step_size = (uint16_t)(bp->memorg.page_size / nop);

	bp->p.ecc_req.strength_per_step = ufprog_pp_read_u8(snand->onfi.data, ONFI_ECC_BITS_OFFS);

	bp->p.ecc_type = ECC_UNKNOWN;

	bp->memorg.planes_per_lun = 1;
	bp->p.memorg = &bp->memorg;
}

bool spi_nand_probe_onfi_generic(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp, uint32_t page,
				 bool fill_all)
{
	uint8_t pp[ONFI_PARAM_PAGE_SIZE * PARAM_PAGE_MIN_COUNT];
	ufprog_status ret;

	/* Disable ECC before reading parameter page */
	STATUS_CHECK_RET(spi_nand_refresh_config(snand));
	STATUS_CHECK_RET(spi_nand_set_config(snand, 0));

	/* Enter OTP mode */
	STATUS_CHECK_GOTO_RET(spi_nand_otp_control_cr_bit6(snand, true), ret, out);

	/* Read parameter page to cache */
	STATUS_CHECK_GOTO_RET(spi_nand_op_read_page_to_cache(snand, page), ret, out);
	STATUS_CHECK_GOTO_RET(spi_nand_wait_busy(snand, SNAND_POLL_MAX_US, NULL), ret, out);

	/* Read cache */
	STATUS_CHECK_GOTO_RET(spi_nand_read_cache_single(snand, 0, sizeof(pp), pp), ret, out);

out:
	/* Leave OTP mode */
	spi_nand_otp_control_cr_bit6(snand, false);

	if (ret)
		return false;

	if (!ufprog_pp_check_recover(pp, PP_CRC_BASE, ONFI_PARAM_PAGE_SIZE, sizeof(pp), ONFI_SIGNATURE)) {
		if (!ufprog_pp_check_recover(pp, PP_CRC_BASE, ONFI_PARAM_PAGE_SIZE, sizeof(pp),
					     SPI_NAND_ONFI_ALT_SIGNATURE))
			return false;
	}

	memcpy(snand->param.onfi, pp, ONFI_PARAM_PAGE_SIZE);
	memcpy(snand->onfi.data, pp, ONFI_PARAM_PAGE_SIZE);
	snand->onfi.valid = true;

	logm_dbg("ONFI parameter page found\n");

	if (!fill_all) {
		logm_dbg("ONFI parameter table will only be used for timing setup\n");
		spi_nand_parse_onfi_fill_time(snand, bp);
	} else {
		spi_nand_parse_onfi_fill(snand, bp);
	}

	return true;
}

static bool spi_nand_probe_onfi(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp, bool fill_all)
{
	return spi_nand_probe_onfi_generic(snand, bp, NAND_OTP_PAGE_PARAM_PAGE, fill_all);
}

static bool spi_nand_setup_ecc_control(struct spi_nand *snand, const struct spi_nand_flash_part *part)
{
	switch (part->ecc_type) {
	case ECC_UNKNOWN:
	case ECC_UNSUPPORTED:
		return true;

	case ECC_ALWAYS_ON:
		snand->ext_param.ops.ecc_control = spi_nand_ecc_control_always_on;
		return true;

	case ECC_CR_BIT4:
		snand->ext_param.ops.ecc_control = spi_nand_ecc_control_cr_bit4;
		return true;

	default:
		logm_err("Invalid configuration for ECC control\n");
		snand->ext_param.ops.ecc_control = NULL;
		return false;
	}
}

static bool spi_nand_setup_quad_enable(struct spi_nand *snand, const struct spi_nand_flash_part *part)
{
	switch (part->qe_type) {
	case QE_UNKNOWN:
	case QE_DONT_CARE:
		return true;

	case QE_CR_BIT0:
		snand->ext_param.ops.quad_enable = spi_nand_quad_enable_cr_bit0;
		return true;

	default:
		logm_err("Invalid configuration for Quad-Enable\n");
		snand->ext_param.ops.quad_enable = NULL;
		return false;
	}
}

static bool spi_nand_setup_otp_control(struct spi_nand *snand, const struct spi_nand_flash_part *part)
{
	switch (part->otp_en_type) {
	case OTP_UNKNOWN:
	case OTP_UNSUPPORTED:
		return true;

	case OTP_CR_BIT6:
		snand->ext_param.ops.otp_control = spi_nand_otp_control_cr_bit6;
		return true;

	default:
		logm_err("Invalid configuration for OTP mode\n");
		snand->ext_param.ops.otp_control = NULL;
		return false;
	}
}

static bool spi_nand_test_io_opcode(struct spi_nand *snand, const struct spi_nand_io_opcode *opcodes,
				    enum spi_mem_io_type io_type, enum ufprog_spi_data_dir data_dir)
{
	struct ufprog_spi_mem_op op = { 0 };
	uint8_t dummy_bytes;

	op.cmd.len = 1;
	op.cmd.buswidth = spi_mem_io_cmd_bw(io_type);
	op.cmd.dtr = spi_mem_io_cmd_dtr(io_type);

	op.addr.len = opcodes[io_type].naddrs;
	op.addr.buswidth = spi_mem_io_addr_bw(io_type);
	op.addr.dtr = spi_mem_io_addr_dtr(io_type);

	if ((opcodes[io_type].ndummy * op.addr.buswidth) % 8)
		return false;

	dummy_bytes = opcodes[io_type].ndummy * op.addr.buswidth / 8;

	op.dummy.len = dummy_bytes;
	op.dummy.buswidth = op.addr.buswidth;
	op.dummy.dtr = op.addr.dtr;

	op.data.len = 1;
	op.data.buswidth = spi_mem_io_data_bw(io_type);
	op.data.dtr = spi_mem_io_data_dtr(io_type);
	op.data.dir = data_dir;

	return ufprog_spi_mem_supports_op(snand->spi, &op);
}

static enum spi_mem_io_type spi_nand_choose_io_type(struct spi_nand *snand, const struct spi_nand_io_opcode *opcodes,
						    uint32_t io_caps, enum ufprog_spi_data_dir data_dir)
{
	enum spi_mem_io_type io_type;

	for (io_type = __SPI_MEM_IO_MAX - 1; io_type >= 0; io_type--) {
		if (!(io_caps & (1 << io_type)))
			continue;

		if (!opcodes[io_type].opcode)
			continue;

		if (spi_nand_test_io_opcode(snand, opcodes, io_type, data_dir))
			return io_type;
	}

	return __SPI_MEM_IO_MAX;
}

static bool spi_nand_test_rd_pl_opcode(struct spi_nand *snand, const struct spi_nand_io_opcode *rd_opcodes,
				       uint32_t rd_io_caps,  const struct spi_nand_io_opcode *pl_opcodes,
				       uint32_t pl_io_caps, bool same_cmd_bw, enum spi_mem_io_type *ret_rd_io_type,
				       enum spi_mem_io_type *ret_pl_io_type)
{
	enum spi_mem_io_type rd_io_type = SPI_MEM_IO_1_1_1, pl_io_type = SPI_MEM_IO_1_1_1;
	uint32_t rd_bw, pl_bw, dis_bw, mask = 0;

	if (!rd_opcodes || !pl_opcodes || !rd_io_caps || !pl_io_caps)
		return false;

	if (ufprog_spi_if_caps(snand->spi) & UFP_SPI_NO_QPI_BULK_READ)
		rd_io_caps &= ~BIT_SPI_MEM_IO_4_4_4;

	rd_io_caps &= snand->allowed_io_caps;
	pl_io_caps &= snand->allowed_io_caps;

	while (rd_io_caps && pl_io_caps) {
		rd_io_type = spi_nand_choose_io_type(snand, rd_opcodes, rd_io_caps, SPI_DATA_IN);
		if (rd_io_type >= __SPI_MEM_IO_MAX)
			return false;

		pl_io_type = spi_nand_choose_io_type(snand, pl_opcodes, pl_io_caps, SPI_DATA_OUT);
		if (pl_io_type >= __SPI_MEM_IO_MAX)
			return false;

		if (!same_cmd_bw)
			break;

		rd_bw = spi_mem_io_cmd_bw(rd_io_type);
		pl_bw = spi_mem_io_cmd_bw(pl_io_type);

		if (rd_bw == pl_bw)
			break;

		if (rd_bw > pl_bw)
			dis_bw = rd_bw;
		else
			dis_bw = pl_bw;

		switch (dis_bw) {
		case 2:
			mask |= BIT_SPI_MEM_IO_2_2_2 | BIT_SPI_MEM_IO_2D_2D_2D;
		case 4:
			mask |= BIT_SPI_MEM_IO_4_4_4 | BIT_SPI_MEM_IO_4D_4D_4D;
		case 8:
			mask |= BIT_SPI_MEM_IO_8_8_8 | BIT_SPI_MEM_IO_8D_8D_8D;
		}

		rd_io_caps &= ~mask;
		pl_io_caps &= ~mask;
	}

	*ret_rd_io_type = rd_io_type;
	*ret_pl_io_type = pl_io_type;

	return true;
}

static bool spi_nand_setup_opcode(struct spi_nand *snand, const struct spi_nand_flash_part *part)
{
	const struct spi_nand_io_opcode *rd_opcodes, *pl_opcodes;
	enum spi_mem_io_type rd_io_type, pl_io_type;
	uint32_t rd_io_caps, pl_io_caps;
	bool ret;

	if (part->rd_opcodes)
		rd_opcodes = part->rd_opcodes;
	else
		rd_opcodes = default_rd_opcodes_4d;

	if (part->pl_opcodes)
		pl_opcodes = part->pl_opcodes;
	else
		pl_opcodes = default_pl_opcodes;

	if (part->rd_io_caps)
		rd_io_caps = part->rd_io_caps;
	else
		rd_io_caps = BIT_SPI_MEM_IO_1_1_1;

	if (part->pl_io_caps)
		pl_io_caps = part->pl_io_caps;
	else
		pl_io_caps = BIT_SPI_MEM_IO_1_1_1;

	ret = spi_nand_test_rd_pl_opcode(snand, rd_opcodes, rd_io_caps, pl_opcodes, pl_io_caps,
					 false, &rd_io_type, &pl_io_type);
	if (!ret) {
		logm_err("Unable to select a proper opcode for read from cache/program load\n");
		return false;
	}

	memcpy(&snand->state.rd_opcode, &rd_opcodes[rd_io_type], sizeof(snand->state.rd_opcode));
	snand->state.rd_io_info = ufprog_spi_mem_io_bus_width_info(rd_io_type);

	logm_dbg("Selected opcode %02Xh for read from cache, I/O type %s, %u dummy byte(s)\n",
		 snand->state.rd_opcode.opcode, ufprog_spi_mem_io_name(rd_io_type),
		 snand->state.rd_opcode.ndummy * spi_mem_io_addr_bw(rd_io_type) / 8);

	memcpy(&snand->state.pl_opcode, &pl_opcodes[pl_io_type], sizeof(snand->state.pl_opcode));
	snand->state.pl_io_info = ufprog_spi_mem_io_bus_width_info(pl_io_type);

	logm_dbg("Selected opcode %02Xh for program load, I/O type %s\n", snand->state.pl_opcode.opcode,
		 ufprog_spi_mem_io_name(pl_io_type));

	return true;
}

static ufprog_status spi_nand_setup_param(struct spi_nand *snand, const struct spi_nand_vendor *vendor,
					  struct spi_nand_flash_part *part)
{
	if (!spi_nand_setup_ecc_control(snand, part))
		return UFP_FAIL;

	if (!spi_nand_setup_quad_enable(snand, part))
		return UFP_FAIL;

	if (!spi_nand_setup_otp_control(snand, part))
		return UFP_FAIL;

	if (part->qe_type == QE_UNKNOWN) {
		part->rd_io_caps &= ~(BIT_SPI_MEM_IO_X4);
		part->pl_io_caps &= ~(BIT_SPI_MEM_IO_X4);
	}

	if (!spi_nand_setup_opcode(snand, part))
		return UFP_FAIL;

	return UFP_OK;
}

static ufprog_status spi_nand_ecc_free_ni(struct ufprog_nand_ecc_chip *ecc)
{
	/* Fake free */
	return UFP_OK;
}

static ufprog_status spi_nand_ecc_decode_page(struct ufprog_nand_ecc_chip *ecc, void *page)
{
	struct spi_nand *snand = ecc_to_spi_nand(ecc);

	return snand->ecc_ret;
}

static const struct nand_ecc_status *spi_nand_ecc_get_status(struct ufprog_nand_ecc_chip *ecc)
{
	struct spi_nand *snand = ecc_to_spi_nand(ecc);

	return snand->ecc_status;
}

void spi_nand_reset_ecc_status(struct spi_nand *snand)
{
	memset(snand->ecc_status, 0,
	       sizeof(*snand->ecc_status) + snand->state.ecc_steps * sizeof(snand->ecc_status->step_bitflips[0]));
}

static ufprog_status spi_nand_ecc_set_enable(struct ufprog_nand_ecc_chip *ecc, bool enable)
{
	struct spi_nand *snand = ecc_to_spi_nand(ecc);

	snand->state.ecc_enabled = enable;

	return UFP_OK;
}

static ufprog_status spi_nand_setup_on_die_ecc(struct spi_nand *snand, const struct spi_nand_vendor *vendor,
					       struct spi_nand_flash_part *part)
{
	memset(&snand->nand.default_bbm_config, 0, sizeof(snand->nand.default_bbm_config));

	ufprog_nand_bbm_add_check_pos(&snand->nand, &snand->ecc.bbm_config.check, snand->nand.memorg.page_size);
	ufprog_nand_bbm_add_mark_pos(&snand->nand, &snand->ecc.bbm_config.mark, snand->nand.memorg.page_size);
	ufprog_nand_bbm_add_page(&snand->nand, &snand->ecc.bbm_config.pages, 0);

	if (part->flags & SNAND_F_BBM_2ND_PAGE)
		ufprog_nand_bbm_add_page(&snand->nand, &snand->ecc.bbm_config.pages, 1);

	snand->ecc.bbm_config.check.width = 8;
	snand->ecc.bbm_config.mark.bytes = 1;
	snand->ecc.bbm_config.flags = ECC_F_BBM_RAW;

	if (part->ecc_type == ECC_UNSUPPORTED || part->ecc_type == ECC_UNKNOWN) {
		snand->ecc.type = NAND_ECC_NONE;
		snand->nand.default_ecc = NULL;
		return UFP_OK;
	}

	snand->ecc.type = NAND_ECC_ON_DIE;
	snand->ecc.name = "On-die";

	snand->ecc.config.step_size = part->ecc_req.step_size;
	snand->ecc.config.strength_per_step = part->ecc_req.strength_per_step;

	/* The On-Die ECC of SPI-NAND has only one page layout */
	snand->ecc.page_layout = part->page_layout;

	snand->state.ecc_steps = part->memorg->page_size / part->ecc_req.step_size;
	snand->ecc_status = calloc(sizeof(snand->ecc_status) +
				   snand->state.ecc_steps * sizeof(snand->ecc_status->step_bitflips[0]), 1);
	if (!snand->ecc_status) {
		logm_err("Failed to allocate memory for ECC status record\n");
		return UFP_NOMEM;
	}

	snand->ecc.free_ni = spi_nand_ecc_free_ni;
	snand->ecc.decode_page = spi_nand_ecc_decode_page;
	snand->ecc.get_status = spi_nand_ecc_get_status;
	snand->ecc.set_enable = spi_nand_ecc_set_enable;

	snand->nand.default_ecc = &snand->ecc;

	return UFP_OK;
}

static ufprog_status spi_nand_setup_param_final(struct spi_nand *snand, const struct spi_nand_vendor *vendor,
						struct spi_nand_flash_part *part, const char *raw_vendor)
{
	uint8_t pl_data_bw, final_data_bw;
	uint32_t max_speed = 0;

	if (!snand->ext_param.ops.chip_setup) {
		if (part->ops && part->ops->chip_setup)
			snand->ext_param.ops.chip_setup = part->ops->chip_setup;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->chip_setup)
			snand->ext_param.ops.chip_setup = vendor->default_part_ops->chip_setup;
	}

	if (!snand->ext_param.ops.select_die) {
		if (part->ops && part->ops->select_die)
			snand->ext_param.ops.select_die = part->ops->select_die;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->select_die)
			snand->ext_param.ops.select_die = vendor->default_part_ops->select_die;
	}

	if (!snand->ext_param.ops.quad_enable && part->qe_type != QE_DONT_CARE) {
		if (part->ops && part->ops->quad_enable)
			snand->ext_param.ops.quad_enable = part->ops->quad_enable;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->quad_enable)
			snand->ext_param.ops.quad_enable = vendor->default_part_ops->quad_enable;
	}

	if (!snand->ext_param.ops.ecc_control && part->ecc_type != ECC_UNSUPPORTED) {
		if (part->ops && part->ops->ecc_control)
			snand->ext_param.ops.ecc_control = part->ops->ecc_control;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->ecc_control)
			snand->ext_param.ops.ecc_control = vendor->default_part_ops->ecc_control;
	}

	if (!snand->ext_param.ops.otp_control && part->otp_en_type != OTP_UNSUPPORTED) {
		if (part->ops && part->ops->otp_control)
			snand->ext_param.ops.otp_control = part->ops->otp_control;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->otp_control)
			snand->ext_param.ops.otp_control = vendor->default_part_ops->otp_control;
	}

	if (!snand->ext_param.ops.check_ecc && part->ecc_type != ECC_UNSUPPORTED) {
		if (part->flags & SNAND_F_EXTENDED_ECC_BFR_8B)
			snand->ext_param.ops.check_ecc = spi_nand_check_extended_ecc_bfr_8b;
		else if (part->ops && part->ops->check_ecc)
			snand->ext_param.ops.check_ecc = part->ops->check_ecc;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->check_ecc)
			snand->ext_param.ops.check_ecc = vendor->default_part_ops->check_ecc;
	}

	if (!snand->ext_param.ops.read_uid) {
		if (part->flags & SNAND_F_GENERIC_UID)
			snand->ext_param.ops.read_uid = spi_nand_read_uid_generic;
		else if (part->ops && part->ops->read_uid)
			snand->ext_param.ops.read_uid = part->ops->read_uid;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->read_uid)
			snand->ext_param.ops.read_uid = vendor->default_part_ops->read_uid;
	}

	if (!snand->ext_param.ops.nor_read_enable && (part->flags & SNAND_F_NOR_READ_CAP)) {
		if (part->ops && part->ops->nor_read_enable)
			snand->ext_param.ops.nor_read_enable = part->ops->nor_read_enable;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->nor_read_enable)
			snand->ext_param.ops.nor_read_enable = vendor->default_part_ops->nor_read_enable;
	}

	if (!snand->ext_param.ops.nor_read_enabled && (part->flags & SNAND_F_NOR_READ_CAP)) {
		if (part->ops && part->ops->nor_read_enabled)
			snand->ext_param.ops.nor_read_enabled = part->ops->nor_read_enabled;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->nor_read_enabled)
			snand->ext_param.ops.nor_read_enabled = vendor->default_part_ops->nor_read_enabled;
	}

	if (!snand->param.max_pp_time_us)
		snand->param.max_pp_time_us = SNAND_POLL_MAX_US;

	if (!snand->param.max_be_time_us)
		snand->param.max_be_time_us = SNAND_POLL_MAX_US;

	if (!snand->param.max_r_time_us)
		snand->param.max_r_time_us = SNAND_POLL_MAX_US;

	final_data_bw = spi_mem_io_info_data_bw(snand->state.rd_io_info);
	pl_data_bw = spi_mem_io_info_data_bw(snand->state.pl_io_info);
	if (final_data_bw < pl_data_bw)
		final_data_bw = pl_data_bw;

	switch (final_data_bw) {
	case 2:
		max_speed = part->max_speed_dual_mhz;
		break;

	case 4:
		max_speed = part->max_speed_quad_mhz;
		break;
	}

	if (!max_speed)
		max_speed = part->max_speed_spi_mhz;

	max_speed *= 1000000;
	if (!max_speed)
		max_speed = snand->max_speed;

	snand->param.max_speed = max_speed;

	snand->state.speed_high = snand->param.max_speed;
	if (snand->state.speed_high > snand->max_speed)
		snand->state.speed_high = snand->max_speed;

	/* Set and read back the real highest/lowest speed */
	STATUS_CHECK_RET(spi_nand_set_high_speed(snand));
	snand->state.speed_high = ufprog_spi_get_speed(snand->spi);

	STATUS_CHECK_RET(spi_nand_set_low_speed(snand));
	snand->state.speed_low = ufprog_spi_get_speed(snand->spi);

	snand->param.flags = part->flags;
	snand->param.vendor_flags = part->vendor_flags;

	memset(snand->param.vendor, 0, sizeof(snand->param.vendor));
	memset(snand->param.model, 0, sizeof(snand->param.model));

	if (vendor)
		snprintf(snand->param.vendor, sizeof(snand->param.vendor), "%s", vendor->name);
	else if (raw_vendor)
		snprintf(snand->param.vendor, sizeof(snand->param.vendor), "%s", raw_vendor);
	else
		snprintf(snand->param.vendor, sizeof(snand->param.vendor), "Unknown (%02X)", snand->nand.id.id[0]);

	if (part->model)
		snprintf(snand->param.model, sizeof(snand->param.model), "%s", part->model);
	else
		snprintf(snand->param.model, sizeof(snand->param.model), "%s", "Unknown");

	/* Fill generic NAND information */
	snand->nand.vendor = snand->param.vendor;
	snand->nand.model = snand->param.model;
	snand->nand.bus_width = final_data_bw;
	snand->nand.otp = part->otp;

	if (snand->onfi.valid)
		snand->nand.bits_per_cell = ufprog_pp_read_u8(snand->onfi.data, PP_BITS_PER_CELL_OFFS);
	else
		snand->nand.bits_per_cell = 1;

	snand->nand.ecc_req.step_size = part->ecc_req.step_size;
	snand->nand.ecc_req.strength_per_step = part->ecc_req.strength_per_step;

	snand->nand.nops = part->nops;
	if (!snand->nand.nops)
		snand->nand.nops = 1;

	if (!snand->nand.otp_ops) {
		if (part->otp_ops)
			snand->nand.otp_ops = part->otp_ops;
		else if (vendor && vendor->default_part_otp_ops)
			snand->nand.otp_ops = vendor->default_part_otp_ops;
	}

	snand->nand.random_page_write = !!(part->flags & SNAND_F_RND_PAGE_WRITE);

	memcpy(&snand->nand.memorg, part->memorg, sizeof(snand->nand.memorg));
	ufprog_nand_update_param(&snand->nand);

	/* Setup generic NAND ops & page cache */
	snand->nand.page_cache[0] = malloc(3 * snand->nand.maux.oob_page_size);
	if (!snand->nand.page_cache[0]) {
		logm_err("Unable to allocate page cache buffer\n");
		return UFP_NOMEM;
	}

	snand->nand.page_cache[1] = (uint8_t *)snand->nand.page_cache[0] + snand->nand.maux.oob_page_size;
	snand->scratch_buffer = (uint8_t *)snand->nand.page_cache[1] + snand->nand.maux.oob_page_size;

	snand->nand.select_die = spi_nand_ops_select_die;
	snand->nand.read_page = spi_nand_ops_read_page;
	snand->nand.write_page = spi_nand_ops_write_page;
	snand->nand.erase_block = spi_nand_ops_erase_block;

	if (snand->ext_param.ops.read_uid)
		snand->nand.read_uid = spi_nand_ops_read_uid;

	if ((snand->param.flags & (SNAND_F_READ_CACHE_RANDOM | SNAND_F_READ_CACHE_SEQ)) &&
	    (snand->config & SPI_NAND_CFG_DIRECT_MULTI_PAGE_READ))
		snand->nand.read_pages = spi_nand_ops_read_pages;

	/* Setup default On-die ECC */
	STATUS_CHECK_RET(spi_nand_setup_on_die_ecc(snand, vendor, part));

	return UFP_OK;
}

static ufprog_status spi_nand_chip_die_setup(struct spi_nand *snand, uint32_t dieidx)
{
	spi_nand_select_die(snand, (uint8_t)dieidx);

	/* Initialize the configuration cache */
	STATUS_CHECK_RET(spi_nand_refresh_config(snand));

	/* Do manufacturer-specific initialization */
	if (snand->ext_param.ops.chip_setup)
		STATUS_CHECK_RET(snand->ext_param.ops.chip_setup(snand));

	/* Disable block protection */
	STATUS_CHECK_RET(spi_nand_unlock(snand));

	/* Enable QSPI if needed */
	if (snand->nand.bus_width >= 4)
		STATUS_CHECK_RET(spi_nand_qspi_enable(snand));

	/* Disable On-die ECC by default */
	spi_nand_ondie_ecc_control(snand, false);

	return UFP_OK;
}

static ufprog_status spi_nand_chip_setup(struct spi_nand *snand)
{
	uint32_t i;

	for (i = snand->nand.memorg.luns_per_cs; i > 0; i--)
		STATUS_CHECK_RET(spi_nand_chip_die_setup(snand, i - 1));

	return UFP_OK;
}

static ufprog_status spi_nand_chip_reset_setup(struct spi_nand *snand)
{
	ufprog_status ret;

	/* Reset SPI-NAND chip */
	ret = spi_nand_chip_reset(snand);
	if (ret) {
		logm_err("Failed to reset SPI-NAND chip\n");
		return ret;
	}

	return spi_nand_chip_setup(snand);
}

static void spi_nand_reset_param(struct spi_nand *snand)
{
	uint32_t i;

	if (snand->nand.page_cache[0])
		free(snand->nand.page_cache[0]);

	for (i = 0; i < ARRAY_SIZE(snand->nand.page_cache); i++)
			snand->nand.page_cache[i] = NULL;

	if (snand->ecc_status) {
		free(snand->ecc_status);
		snand->ecc_status = NULL;
	}

	snand->scratch_buffer = NULL;

	memset(&snand->nand, 0, sizeof(snand->nand));
	memset(&snand->ecc, 0, sizeof(snand->ecc));
	memset(&snand->onfi, 0, sizeof(snand->onfi));
	memset(&snand->param, 0, sizeof(snand->param));
	memset(&snand->ext_param, 0, sizeof(snand->ext_param));
	memset(&snand->state, 0, sizeof(snand->state));
}

static ufprog_status spi_nand_pre_init(struct spi_nand *snand)
{
	ufprog_status ret;

	snand->state.speed_low = SNAND_SPEED_LOW;
	snand->state.speed_high = SNAND_SPEED_LOW;

	STATUS_CHECK_RET(ufprog_spi_set_cs_pol(snand->spi, false));

	ret = ufprog_spi_set_mode(snand->spi, SPI_MODE_0);
	if (ret && ret != UFP_UNSUPPORTED) {
		ret = ufprog_spi_set_mode(snand->spi, SPI_MODE_3);
		if (ret && ret != UFP_UNSUPPORTED) {
			logm_err("Cannot set SPI controller to use either mode 0 or mode 3\n");
			return ret;
		}
	}

	return UFP_OK;
}

static ufprog_status spi_nand_init(struct spi_nand *snand, struct spi_nand_vendor_part *vp,
				   struct spi_nand_flash_part_blank *bp)
{
	if (vp->vendor && vp->vendor->default_part_fixups && vp->vendor->default_part_fixups->pre_param_setup)
		STATUS_CHECK_RET(vp->vendor->default_part_fixups->pre_param_setup(snand, bp));

	if (vp->part && vp->part->fixups && vp->part->fixups->pre_param_setup)
		STATUS_CHECK_RET(vp->part->fixups->pre_param_setup(snand, bp));

	STATUS_CHECK_RET(spi_nand_setup_param(snand, vp->vendor, &bp->p));

	if (vp->vendor && vp->vendor->default_part_fixups && vp->vendor->default_part_fixups->post_param_setup)
		STATUS_CHECK_RET(vp->vendor->default_part_fixups->post_param_setup(snand, bp));

	if (vp->part && vp->part->fixups && vp->part->fixups->post_param_setup)
		STATUS_CHECK_RET(vp->part->fixups->post_param_setup(snand, bp));

	if (!bp->p.model && bp->model[0])
		bp->p.model = bp->model;

	STATUS_CHECK_RET(spi_nand_setup_param_final(snand, vp->vendor, &bp->p, bp->vendor[0] ? bp->vendor : NULL));

	if (vp->vendor && vp->vendor->default_part_fixups && vp->vendor->default_part_fixups->pre_chip_setup)
		STATUS_CHECK_RET(vp->vendor->default_part_fixups->pre_chip_setup(snand));

	if (vp->part && vp->part->fixups && vp->part->fixups->pre_chip_setup)
		STATUS_CHECK_RET(vp->part->fixups->pre_chip_setup(snand));

	memcpy(&snand->nand.default_bbm_config, &snand->ecc.bbm_config, sizeof(snand->ecc.bbm_config));
	STATUS_CHECK_RET(ufprog_nand_set_ecc(&snand->nand, snand->nand.default_ecc));

	logm_dbg("Vendor: %s, Model: %s\n", snand->param.vendor, snand->param.model);

	STATUS_CHECK_RET(spi_nand_chip_setup(snand));

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nand_list_parts(struct spi_nand_part_list **outlist, const char *vendorid,
						    const char *match)
{
	struct spi_nand_part_list *partlist;
	uint32_t count;

	if (!outlist)
		return UFP_INVALID_PARAMETER;

	count = spi_nand_list_parts(vendorid, match, NULL, NULL);

	partlist = malloc(sizeof(*partlist) + count * sizeof(partlist->list[0]));
	if (!partlist) {
		logm_err("No memory for flash part list\n");
		return UFP_NOMEM;
	}

	partlist->num = count;

	spi_nand_list_parts(vendorid, match, NULL, partlist->list);

	*outlist = partlist;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nand_probe(struct spi_nand *snand, struct spi_nand_part_list **outlist,
					       struct nand_id *retid)
{
	struct spi_nand_vendor_part vp = { NULL };
	struct spi_nand_part_list *partlist;
	ufprog_status ret = UFP_OK;
	uint32_t count;

	if (!snand || !outlist)
		return UFP_INVALID_PARAMETER;

	spi_nand_reset_param(snand);

	STATUS_CHECK_RET(spi_nand_pre_init(snand));

	ufprog_spi_nand_bus_lock(snand);

	/* Reset SPI-NAND chip */
	ret = spi_nand_chip_reset(snand);
	if (!ret) {
		/* Probe SPI-NAND flash by JEDEC ID */
		if (!spi_nand_probe_jedec_id(snand, &vp))
			ret = UFP_NOT_EXIST;
	} else {
		logm_err("Failed to reset SPI-NAND chip\n");
	}

	ufprog_spi_nand_bus_unlock(snand);

	if (ret)
		return ret;

	count = spi_nand_list_parts(NULL, NULL, &vp.part->id, NULL);

	partlist = malloc(sizeof(*partlist) + count * sizeof(partlist->list[0]));
	if (!partlist) {
		logm_err("No memory for flash part list\n");
		return UFP_NOMEM;
	}

	partlist->num = count;

	spi_nand_list_parts(NULL, NULL, &vp.part->id, partlist->list);

	*outlist = partlist;

	if (retid) {
		memset(retid, 0, sizeof(*retid));
		retid->len = vp.part->id.val.len;
		memcpy(retid->id, vp.part->id.val.id, retid->len);
	}

	return 0;
}

ufprog_status UFPROG_API ufprog_spi_nand_free_list(void *list)
{
	if (list)
		free(list);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nand_part_init(struct spi_nand *snand, const char *vendorid, const char *part)
{
	struct spi_nand_vendor_part vpreq = { 0 };
	struct spi_nand_flash_part_blank bp;
	uint8_t id[NAND_ID_MAX_LEN];
	ufprog_status ret;
	size_t namelen;
	uint32_t i;

	if (!snand || !part)
		return UFP_INVALID_PARAMETER;

	spi_nand_reset_param(snand);

	STATUS_CHECK_RET(spi_nand_pre_init(snand));

	/* Find the requested vendor */
	if (vendorid) {
		vpreq.vendor = spi_nand_find_vendor_by_id(vendorid);
		logm_err("Requested vendor name does not exist\n");
		return UFP_NOT_EXIST;
	}

	/* Find the requested part */
	if (!vpreq.vendor) {
		if (!spi_nand_find_vendor_part_by_name(part, &vpreq))
			vpreq.part = NULL;
	} else {
		vpreq.part = spi_nand_vendor_find_part_by_name(part, vpreq.vendor);
	}

	if (!vpreq.part) {
		logm_err("Requested part name does not exist\n");
		return UFP_NOT_EXIST;
	}

	if (vpreq.part->flags & SNAND_F_NO_OP) {
		logm_err("This part can not be used\n");
		return UFP_FLASH_PART_NOT_SPECIFIED;
	}

	ufprog_spi_nand_bus_lock(snand);

	/* Check if ID matches */
	switch (vpreq.part->id.type) {
	case SNAND_ID_DUMMY:
		ret = spi_nand_read_id(snand, id, vpreq.part->id.val.len);
		break;

	case SNAND_ID_ADDR:
		ret = spi_nand_read_id_addr(snand, id, vpreq.part->id.val.len, 0);
		break;

	case SNAND_ID_DIRECT:
		ret = spi_nand_read_id_direct(snand, id, vpreq.part->id.val.len);
		break;

	default:
		ret = UFP_UNSUPPORTED;
	}

	if (ret) {
		logm_err("Failed to read JEDEC ID\n");
		goto out;
	}

	if (memcmp(id, vpreq.part->id.val.id, vpreq.part->id.val.len)) {
		logm_err("Requested part JEDEC ID mismatch\n");
		ret = UFP_FLASH_PART_MISMATCH;
		goto out;
	}

	spi_nand_prepare_blank_part(&bp, vpreq.part);

	if (!(vpreq.part->flags & SNAND_F_NO_PP))
		spi_nand_probe_onfi(snand, &bp, false);

	memcpy(&snand->nand.id, &vpreq.part->id.val, sizeof(vpreq.part->id.val));

	if (strcasecmp(part, vpreq.part->model)) {
		for (i = 0; i < vpreq.part->alias->num; i++) {
			if (!strcasecmp(part, vpreq.part->alias->items[i].model)) {
				namelen = strlen(vpreq.part->alias->items[i].model);
				if (namelen >= sizeof(bp.model))
					namelen = sizeof(bp.model) - 1;

				memcpy(bp.model, vpreq.part->alias->items[i].model, namelen);
				bp.model[namelen] = 0;

				break;
			}
		}
	}

	ret = spi_nand_init(snand, &vpreq, &bp);

out:
	ufprog_spi_nand_bus_unlock(snand);

	STATUS_CHECK_RET(spi_nand_set_high_speed(snand));

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nand_probe_init(struct spi_nand *snand)
{
	struct spi_nand_flash_part_blank bp;
	struct spi_nand_vendor_part vp;
	bool pp_probed = false;
	ufprog_status ret;
	char idstr[20];

	if (!snand)
		return UFP_INVALID_PARAMETER;

	spi_nand_reset_param(snand);

	STATUS_CHECK_RET(spi_nand_pre_init(snand));

	ufprog_spi_nand_bus_lock(snand);

	/* Reset SPI-NAND chip */
	ret = spi_nand_chip_reset(snand);
	if (ret) {
		logm_err("Failed to reset SPI-NAND chip\n");
		return ret;
	}

	/* Probe SPI-NAND flash by JEDEC ID */
	spi_nand_probe_jedec_id(snand, &vp);

	if (vp.part && (vp.part->flags & SNAND_F_NO_OP)) {
		logm_err("This part does not support auto probing. Please manually select a matched part\n");
		return UFP_FLASH_PART_NOT_SPECIFIED;
	}

	spi_nand_prepare_blank_part(&bp, vp.part);

	/* Read parameter page. This is mandatory if JEDEC ID probing failed. */
	if (vp.part && !(vp.part->flags & SNAND_F_NO_PP))
		pp_probed = spi_nand_probe_onfi(snand, &bp, !vp.part);

	if (!vp.part && !pp_probed) {
		logm_errdbg("Unable to identify SPI-NAND chip\n");
		ret = UFP_FLASH_PART_NOT_RECOGNISED;
		goto out;
	}

	if (pp_probed) {
		if (!vp.part) {
			/*
			 * Only parameter page was probed and we havn't got the correct JEDEC ID here.
			 * Since we don't know the ID read type, we have to try all three types and check
			 * if the vendor ID matches.
			 */
			snand->nand.id.len = SPI_NAND_ID_LEN;

			ret = spi_nand_read_match_jedec_id(snand, onfi_read_mfr_id(snand->onfi.data),
							   snand->nand.id.id);
			if (ret) {
				logm_err("Unable to read correct JEDEC ID\n");
				goto out;
			}

			bin_to_hex_str(idstr, sizeof(idstr), snand->nand.id.id, snand->nand.id.len, true, true);
			logm_dbg("JEDEC ID: %s\n", idstr);
		}

		if (!vp.vendor)
			vp.vendor = spi_nand_find_vendor(snand->nand.id.id[0]);

		if (vp.vendor && !vp.part) {
			if (vp.vendor->ops->pp_post_init)
				STATUS_CHECK_GOTO_RET(vp.vendor->ops->pp_post_init(snand, &bp), ret, out);
		}
	}

	ret = spi_nand_init(snand, &vp, &bp);

out:
	ufprog_spi_nand_bus_unlock(snand);

	STATUS_CHECK_RET(spi_nand_set_high_speed(snand));

	return ret;
}

ufprog_status spi_nand_reprobe_part(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp,
				    const struct spi_nand_vendor *vendor, const char *part)
{
	struct spi_nand_vendor_part vp;

	if (!spi_nand_find_vendor_part_by_name(part, &vp)) {
		logm_err("Failed to find part %s\n", part);
		return UFP_FAIL;
	}

	spi_nand_prepare_blank_part(bp, vp.part);

	if (!vendor)
		vendor = vp.vendor;

	return UFP_OK;
}

ufprog_bool UFPROG_API ufprog_spi_nand_valid(struct spi_nand *snand)
{
	if (!snand)
		return false;

	return snand->nand.memorg.page_size > 0;
}

uint32_t UFPROG_API ufprog_spi_nand_flash_param_signature(struct spi_nand *snand)
{
	uint32_t crc = 0;

	if (!snand)
		return 0;

	if (!snand->nand.memorg.page_size)
		return 0;

	crc = crc32(crc, &snand->param, sizeof(snand->param));

	if (snand->onfi.valid)
		crc = crc32(crc, snand->onfi.data, sizeof(snand->onfi.data));

	if (snand->nand.otp)
		crc = crc32(crc, snand->nand.otp, sizeof(*snand->nand.otp));

	return crc;
}

ufprog_status UFPROG_API ufprog_spi_nand_info(struct spi_nand *snand, struct spi_nand_info *info)
{
	if (!snand || !info)
		return UFP_INVALID_PARAMETER;

	memset(info, 0, sizeof(*info));

	if (!snand->nand.memorg.page_size)
		return UFP_FLASH_NOT_PROBED;

	info->signature = ufprog_spi_nand_flash_param_signature(snand);
	info->max_speed = snand->param.max_speed;
	info->rd_io_info = snand->state.rd_io_info;
	info->pl_io_info = snand->state.pl_io_info;
	info->onfi_data = snand->onfi.valid ? snand->onfi.data : NULL;

	return UFP_OK;
}

ufprog_status spi_nand_page_op(struct spi_nand *snand, uint32_t page, uint8_t cmd)
{
	struct ufprog_spi_mem_op op = SNAND_PAGE_OP(cmd, page);

	return ufprog_spi_mem_exec_op(snand->spi, &op);
}

static inline ufprog_status spi_nand_op_read_page_to_cache(struct spi_nand *snand, uint32_t page)
{
	return spi_nand_page_op(snand, page, SNAND_CMD_READ_TO_CACHE);
}

static inline ufprog_status spi_nand_op_program_execute(struct spi_nand *snand, uint32_t page)
{
	return spi_nand_page_op(snand, page, SNAND_CMD_PROGRAM_EXECUTE);
}

static inline ufprog_status spi_nand_op_block_erase(struct spi_nand *snand, uint32_t page)
{
	return spi_nand_page_op(snand, page, SNAND_CMD_BLOCK_ERASE);
}

ufprog_status spi_nand_read_cache_custom(struct spi_nand *snand, const struct spi_nand_io_opcode *opcode,
					 uint32_t io_info, uint32_t column, uint32_t len, void *data)
{
	uint32_t ndummy = opcode->ndummy * spi_mem_io_info_addr_bw(io_info) / 8;

	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(opcode->opcode, spi_mem_io_info_cmd_bw(io_info)),
		SPI_MEM_OP_ADDR(opcode->naddrs, column, spi_mem_io_info_addr_bw(io_info)),
		SPI_MEM_OP_DUMMY((uint8_t)ndummy, spi_mem_io_info_addr_bw(io_info)),
		SPI_MEM_OP_DATA_IN(len, data, spi_mem_io_info_data_bw(io_info))
	);

	while (len) {
		STATUS_CHECK_RET(ufprog_spi_mem_adjust_op_size(snand->spi, &op));
		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snand->spi, &op));

		op.data.buf.rx = (void *)((uintptr_t)op.data.buf.rx + op.data.len);

		column += (uint32_t)op.data.len;
		op.addr.val = column;

		len -= (uint32_t)op.data.len;
		op.data.len = len;
	}

	return UFP_OK;
}

static inline ufprog_status spi_nand_read_cache(struct spi_nand *snand, uint32_t column, uint32_t len, void *data)
{
	return spi_nand_read_cache_custom(snand, &snand->state.rd_opcode, snand->state.rd_io_info, column, len, data);
}

ufprog_status spi_nand_read_cache_single(struct spi_nand *snand, uint32_t column, uint32_t len, void *data)
{
	return spi_nand_read_cache_custom(snand, &default_rd_opcodes_4d[SPI_MEM_IO_1_1_1],
					  ufprog_spi_mem_io_bus_width_info(SPI_MEM_IO_1_1_1), column, len, data);
}

static ufprog_status spi_nand_die_read_page(struct spi_nand *snand, uint32_t page, uint32_t column, uint32_t len,
					    void *data, bool check_ecc)
{
	ufprog_status ret;

	column |= spi_nand_get_plane_address(snand, page);

	STATUS_CHECK_RET(spi_nand_op_read_page_to_cache(snand, page));
	ret = spi_nand_wait_busy(snand, snand->param.max_r_time_us, NULL);
	if (ret) {
		logm_err("Read to cache command timed out in page %u\n", page);
		return ret;
	}

	if (check_ecc) {
		snand->ecc_ret = snand->ext_param.ops.check_ecc(snand);
		switch (snand->ecc_ret) {
		case UFP_OK:
		case UFP_ECC_CORRECTED:
		case UFP_ECC_UNCORRECTABLE:
			break;

		default:
			logm_err("Failed to read ECC status\n");
			return snand->ecc_ret;
		}
	} else {
		snand->ecc_ret = UFP_OK;
	}

	STATUS_CHECK_RET(spi_nand_read_cache(snand, column, len, data));

	return ret;
}

static ufprog_status spi_nand_die_read_pages(struct spi_nand *snand, uint32_t page, uint32_t count, void *buf,
					     bool check_ecc, uint32_t flags, uint32_t *retcount)
{
	uint8_t faddr, crbsym, *p = buf;
	uint32_t column, rdcnt = 0;
	bool seq_mode = false;
	ufprog_status ret;

	if (snand->param.flags & SNAND_F_READ_CACHE_SEQ) {
		seq_mode = true;
		faddr = snand->state.seq_rd_feature_addr;
		crbsym = snand->state.seq_rd_crbsy_mask;
	} else {
		faddr = SPI_NAND_FEATURE_STATUS_ADDR;
		crbsym = SPI_NAND_STATUS_CRBSY;
	}

	STATUS_CHECK_RET(spi_nand_op_read_page_to_cache(snand, page));

	ret = spi_nand_wait_busy(snand, snand->param.max_r_time_us, NULL);
	if (ret) {
		logm_err("Read to cache command timed out in page %u\n", page);
		return ret;
	}

	while (count > 1) {
		if (check_ecc) {
			snand->ecc_ret = snand->ext_param.ops.check_ecc(snand);
			if (snand->ecc_ret) {
				if (snand->ecc_ret == UFP_ECC_CORRECTED || snand->ecc_ret == UFP_ECC_UNCORRECTABLE) {
					ufprog_nand_print_ecc_result(&snand->nand, page);

					if (ret == UFP_ECC_UNCORRECTABLE) {
						if (!(flags & NAND_READ_F_IGNORE_ECC_ERROR))
							goto cleanup;
					}
				} else if (!(flags & NAND_READ_F_IGNORE_IO_ERROR)) {
					goto cleanup;
				}
			}
		}

		snand->ecc_ret = UFP_OK;

		if (seq_mode)
			STATUS_CHECK_GOTO_RET(spi_nand_issue_single_opcode(snand, SNAND_CMD_READ_FROM_CACHE_SEQ), ret, cleanup);
		else
			STATUS_CHECK_GOTO_RET(spi_nand_page_op(snand, page + 1, SNAND_CMD_READ_FROM_CACHE_RANDOM), ret, cleanup);

		ret = spi_nand_wait_busy_bit(snand, faddr, crbsym, snand->param.max_r_time_us, NULL);
		if (ret) {
			logm_err("Read to cache random command timed out in page %u\n", page + 1);
			goto cleanup;
		}

		column = spi_nand_get_plane_address(snand, page);

		STATUS_CHECK_GOTO_RET(spi_nand_read_cache(snand, column, snand->nand.maux.oob_page_size, p), ret, cleanup);

		ret = spi_nand_wait_busy(snand, snand->param.max_r_time_us, NULL);
		if (ret) {
			logm_err("Read to cache command timed out in page %u\n", page + 1);
			goto cleanup;
		}

		rdcnt++;
		page++;
		count--;
		p += snand->nand.maux.oob_page_size;
	}

	STATUS_CHECK_GOTO_RET(spi_nand_issue_single_opcode(snand, SNAND_CMD_READ_FROM_CACHE_END), ret, cleanup);

	ret = spi_nand_wait_busy_bit(snand, faddr, crbsym, snand->param.max_r_time_us, NULL);;
	if (ret) {
		logm_err("Read to cache random command timed out in page %u\n", page + 1);
		goto cleanup;
	}

	column = spi_nand_get_plane_address(snand, page);

	STATUS_CHECK_GOTO_RET(spi_nand_read_cache(snand, column, snand->nand.maux.oob_page_size, p), ret, cleanup);

	if (retcount)
		*retcount = rdcnt + 1;

	return UFP_OK;

cleanup:
	spi_nand_chip_reset_setup(snand);

	if (retcount)
		*retcount = rdcnt;

	return ret;
}

static ufprog_status spi_nand_chip_read_page(struct spi_nand *snand, uint32_t page, uint32_t column, uint32_t len,
					     void *data, bool enable_ecc)
{
	STATUS_CHECK_RET(spi_nand_select_die_page(snand, &page));

	STATUS_CHECK_RET(spi_nand_ondie_ecc_control(snand, enable_ecc));

	return spi_nand_die_read_page(snand, page, column, len, data, enable_ecc);
}

static ufprog_status spi_nand_ops_read_page(struct nand_chip *nand, uint32_t page, uint32_t column, uint32_t len,
					    void *buf)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);

	return spi_nand_chip_read_page(snand, page, column, len, buf, snand->state.ecc_enabled);
}

static ufprog_status spi_nand_chip_read_pages(struct spi_nand *snand, uint32_t page, uint32_t count, void *buf,
					      bool enable_ecc, uint32_t flags, uint32_t *retcount)
{
	STATUS_CHECK_RET(spi_nand_select_die_page(snand, &page));

	STATUS_CHECK_RET(spi_nand_ondie_ecc_control(snand, enable_ecc));

	return spi_nand_die_read_pages(snand, page, count, buf, enable_ecc, flags, retcount);
}

static ufprog_status spi_nand_ops_read_pages(struct nand_chip *nand, uint32_t page, uint32_t count, void *buf,
					     uint32_t flags, uint32_t *retcount)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);
	uint32_t start_die, end_die;

	if (count == 1)
		return spi_nand_chip_read_page(snand, page, 0, nand->maux.oob_page_size, buf, snand->state.ecc_enabled);

	start_die = page >> (nand->maux.lun_shift - nand->maux.page_shift);
	end_die = (page + count - 1) >> (nand->maux.lun_shift - nand->maux.page_shift);

	if (start_die != end_die) {
		logm_err("Multi-page read can only be operated in the same die.\n");
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;
	}

	return spi_nand_chip_read_pages(snand, page, count, buf, snand->state.ecc_enabled, flags, retcount);
}

ufprog_status spi_nand_program_load_custom(struct spi_nand *snand, const struct spi_nand_io_opcode *opcode,
					   uint32_t io_info, uint32_t column, uint32_t len, const void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(opcode->opcode, spi_mem_io_info_cmd_bw(io_info)),
		SPI_MEM_OP_ADDR(opcode->naddrs, column, spi_mem_io_info_addr_bw(io_info)),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(len, data, spi_mem_io_info_data_bw(io_info))
	);

	while (len) {
		STATUS_CHECK_RET(ufprog_spi_mem_adjust_op_size(snand->spi, &op));
		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snand->spi, &op));

		op.data.buf.tx = (const void *)((uintptr_t)op.data.buf.tx + op.data.len);

		column += (uint32_t)op.data.len;
		op.addr.val = column;

		len -= (uint32_t)op.data.len;
		op.data.len = len;
	}

	return UFP_OK;
}

static ufprog_status spi_nand_program_load(struct spi_nand *snand, uint32_t column, uint32_t len, const void *data)
{
	return spi_nand_program_load_custom(snand, &snand->state.pl_opcode, snand->state.pl_io_info, column, len, data);
}

ufprog_status spi_nand_program_load_single(struct spi_nand *snand, uint32_t column, uint32_t len, const void *data)
{
	return spi_nand_program_load_custom(snand, &default_pl_opcodes[SPI_MEM_IO_1_1_1],
					    ufprog_spi_mem_io_bus_width_info(SPI_MEM_IO_1_1_1), column, len, data);
}

static ufprog_status spi_nand_die_write_page(struct spi_nand *snand, uint32_t page, uint32_t column, uint32_t len,
					     const void *data)
{
	ufprog_status ret;
	uint8_t sr;

	column |= spi_nand_get_plane_address(snand, page);

	STATUS_CHECK_RET(spi_nand_write_enable(snand));
	STATUS_CHECK_GOTO_RET(spi_nand_program_load(snand, column, len, data), ret, errout);
	STATUS_CHECK_GOTO_RET(spi_nand_op_program_execute(snand, page), ret, errout);

	ret = spi_nand_wait_busy(snand, snand->param.max_pp_time_us, &sr);
	if (ret) {
		logm_err("Page program command timed out in page %u\n", page);
		goto errout;
	}

	if (sr & SPI_NAND_STATUS_PROGRAM_FAIL) {
		logm_err("Page program failed in page %u\n", page);
		ret = UFP_FLASH_PROGRAM_FAILED;
		goto errout;
	}

	return UFP_OK;

errout:
	spi_nand_write_disable(snand);

	return ret;
}

static ufprog_status spi_nand_chip_write_page(struct spi_nand *snand, uint32_t page, uint32_t column, uint32_t len,
					      const void *data, bool enable_ecc)
{
	STATUS_CHECK_RET(spi_nand_select_die_page(snand, &page));

	STATUS_CHECK_RET(spi_nand_ondie_ecc_control(snand, enable_ecc));

	return spi_nand_die_write_page(snand, page, column, len, data);
}

static ufprog_status spi_nand_ops_write_page(struct nand_chip *nand, uint32_t page, uint32_t column, uint32_t len,
					     const void *buf)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);

	return spi_nand_chip_write_page(snand, page, column, len, buf, snand->state.ecc_enabled);
}

static ufprog_status spi_nand_die_erase_block(struct spi_nand *snand, uint32_t page)
{
	struct nand_chip *nand = &snand->nand;
	ufprog_status ret;
	uint32_t block;
	uint8_t sr;

	block = page >> (nand->maux.block_shift - nand->maux.page_shift);

	STATUS_CHECK_RET(spi_nand_write_enable(snand));
	STATUS_CHECK_GOTO_RET(spi_nand_op_block_erase(snand, page), ret, errout);

	ret = spi_nand_wait_busy(snand, snand->param.max_be_time_us, &sr);
	if (ret) {
		logm_err("Block erase command timed out on block %u\n", block);
		goto errout;
	}

	if (sr & SPI_NAND_STATUS_ERASE_FAIL) {
		logm_err("Block erase failed on block %u\n", block);
		ret = UFP_FLASH_ERASE_FAILED;
		goto errout;
	}

	return UFP_OK;

errout:
	spi_nand_write_disable(snand);

	return ret;
}

static ufprog_status spi_nand_chip_erase_block(struct spi_nand *snand, uint32_t page)
{
	STATUS_CHECK_RET(spi_nand_select_die_page(snand, &page));

	return spi_nand_die_erase_block(snand, page);
}

static ufprog_status spi_nand_ops_erase_block(struct nand_chip *nand, uint32_t page)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);

	return spi_nand_chip_erase_block(snand, page);
}

static ufprog_status spi_nand_ops_select_die(struct nand_chip *nand, uint32_t ce, uint32_t lun)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);

	return spi_nand_select_die(snand, (uint8_t)lun);
}

static ufprog_status spi_nand_ops_read_uid(struct nand_chip *nand, void *data, uint32_t *retlen)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);

	return snand->ext_param.ops.read_uid(snand, data, retlen);
}

ufprog_status spi_nand_read_uid_otp(struct spi_nand *snand, uint32_t page, void *data, uint32_t *retlen)
{
	uint8_t *p, *cmp, uid_data[NAND_DEFAULT_UID_LEN * 2 * NAND_DEFAULT_UID_REPEATS];
	ufprog_status ret;
	uint32_t i, j;

	if (retlen)
		*retlen = NAND_DEFAULT_UID_LEN;

	if (!data)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nand_set_low_speed(snand));

	/* Disable ECC before reading parameter page */
	STATUS_CHECK_RET(spi_nand_ondie_ecc_control(snand, false));

	/* Enter OTP mode */
	STATUS_CHECK_GOTO_RET(spi_nand_otp_control(snand, true), ret, out);

	/* Read parameter page to cache */
	STATUS_CHECK_GOTO_RET(spi_nand_op_read_page_to_cache(snand, page), ret, out);
	STATUS_CHECK_GOTO_RET(spi_nand_wait_busy(snand, SNAND_POLL_MAX_US, NULL), ret, out);

	/* Read cache */
	STATUS_CHECK_GOTO_RET(spi_nand_read_cache_single(snand, 0, sizeof(uid_data), uid_data), ret, out);

out:
	/* Leave OTP mode */
	spi_nand_otp_control(snand, false);

	if (ret)
		return ret;

	for (i = 0, p = uid_data; i < NAND_DEFAULT_UID_REPEATS; i++, p += NAND_DEFAULT_UID_LEN * 2) {
		cmp = p + NAND_DEFAULT_UID_LEN;

		for (j = 0; j < NAND_DEFAULT_UID_LEN; j++) {
			if ((p[i] ^ cmp[i]) != 0xff)
				break;
		}

		if (j == NAND_DEFAULT_UID_LEN) {
			memcpy(data, p, NAND_DEFAULT_UID_LEN);
			return UFP_OK;
		}
	}

	return UFP_DATA_VERIFICATION_FAIL;
}

static ufprog_status spi_nand_read_uid_generic(struct spi_nand *snand, void *data, uint32_t *retlen)
{
	return spi_nand_read_uid_otp(snand, NAND_OTP_PAGE_UID, data, retlen);
}

ufprog_bool UFPROG_API ufprog_spi_nand_supports_nor_read(struct spi_nand *snand)
{
	if (!snand)
		return false;

	if (!snand->ext_param.ops.nor_read_enable)
		return false;

	return true;
}

ufprog_status UFPROG_API ufprog_spi_nand_enable_nor_read(struct spi_nand *snand)
{
	if (!snand)
		return UFP_INVALID_PARAMETER;

	if (!snand->ext_param.ops.nor_read_enable)
		return UFP_UNSUPPORTED;

	return snand->ext_param.ops.nor_read_enable(snand);
}

ufprog_status UFPROG_API ufprog_spi_nand_nor_read_enabled(struct spi_nand *snand, ufprog_bool *retenabled)
{
	if (!snand)
		return UFP_INVALID_PARAMETER;

	if (!snand->ext_param.ops.nor_read_enabled)
		return UFP_UNSUPPORTED;

	return snand->ext_param.ops.nor_read_enabled(snand, retenabled);
}
