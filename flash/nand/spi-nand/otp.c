// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash OTP page operations
 */

#include <string.h>
#include <ufprog/log.h>
#include "vendor-micron.h"
#include "core.h"
#include "otp.h"

ufprog_status spi_nand_otp_read(struct nand_chip *nand, uint32_t index, uint32_t column, uint32_t len, void *data)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);
	ufprog_status ret;

	STATUS_CHECK_RET(spi_nand_set_low_speed(snand));

	/* Enter OTP mode */
	STATUS_CHECK_GOTO_RET(spi_nand_otp_control(snand, true), ret, out);

	/* Read OTP page to cache */
	STATUS_CHECK_GOTO_RET(spi_nand_page_op(snand, index, SNAND_CMD_READ_TO_CACHE), ret, out);
	STATUS_CHECK_GOTO_RET(spi_nand_wait_busy(snand, SNAND_POLL_MAX_US, NULL), ret, out);

	if (snand->state.ecc_enabled) {
		snand->ecc_ret = snand->ext_param.ops.check_ecc(snand);
		switch (snand->ecc_ret) {
		case UFP_OK:
		case UFP_ECC_CORRECTED:
		case UFP_ECC_UNCORRECTABLE:
			break;

		default:
			logm_err("Failed to read ECC status\n");
			ret =snand->ecc_ret;
			goto out;
		}
	} else {
		snand->ecc_ret = UFP_OK;
	}

	/* Read cache */
	STATUS_CHECK_GOTO_RET(spi_nand_read_cache_single(snand, column, len, data), ret, out);

out:
	/* Leave OTP mode */
	spi_nand_otp_control(snand, false);

	return ret;
}

ufprog_status spi_nand_otp_write(struct nand_chip *nand, uint32_t index, uint32_t column, uint32_t len,
				 const void *data)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);
	ufprog_status ret;
	uint8_t sr;

	STATUS_CHECK_RET(spi_nand_set_low_speed(snand));

	/* Enter OTP mode */
	STATUS_CHECK_GOTO_RET(spi_nand_otp_control(snand, true), ret, out);

	STATUS_CHECK_RET(spi_nand_write_enable(snand));

	/* Write OTP page */
	STATUS_CHECK_GOTO_RET(spi_nand_program_load_single(snand, column, len, data), ret, out);
	STATUS_CHECK_GOTO_RET(spi_nand_page_op(snand, index, SNAND_CMD_PROGRAM_EXECUTE), ret, out);

	ret = spi_nand_wait_busy(snand, SNAND_POLL_MAX_US, &sr);
	if (ret) {
		logm_err("OTP Page program command timed out in index %u\n", index);
		goto out;
	}

	if (sr & SPI_NAND_STATUS_PROGRAM_FAIL) {
		logm_err("OTP Page program failed in index %u\n", index);
		ret = UFP_FLASH_PROGRAM_FAILED;
		goto out;
	}

	ret = UFP_OK;

out:
	/* Leave OTP mode */
	spi_nand_otp_control(snand, false);

	spi_nand_write_disable(snand);

	return ret;
}

ufprog_status spi_nand_otp_lock(struct nand_chip *nand)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);
	ufprog_bool locked = false;
	ufprog_status ret;

	ret = spi_nand_otp_locked(nand, &locked);
	if (ret)
		return ret;

	if (locked)
		return UFP_OK;

	/* Enter OTP mode */
	STATUS_CHECK_GOTO_RET(spi_nand_otp_control(snand, true), ret, out);

	/* Set OTP lock bit */
	ret = spi_nand_update_config(snand, 0, SPI_NAND_CONFIG_OTP_LOCK);
	if (ret) {
		logm_err("Failed to set OTP lock bit\n");
		goto out;
	}

	STATUS_CHECK_GOTO_RET(spi_nand_write_enable(snand), ret, out);

	STATUS_CHECK_GOTO_RET(spi_nand_page_op(snand, 0, SNAND_CMD_PROGRAM_EXECUTE), ret, out);

	ret = spi_nand_wait_busy(snand, SNAND_POLL_MAX_US, NULL);
	if (ret) {
		logm_err("OTP lock command timed out\n");
		goto out;
	}

	ret = UFP_OK;

out:
	/* Leave OTP mode */
	spi_nand_otp_control(snand, false);

	spi_nand_write_disable(snand);

	if (!ret) {
		ret = spi_nand_otp_locked(nand, &locked);
		if (ret)
			return ret;

		if (!locked) {
			logm_err("Failed to set OTP lock bit\n");
			ret = UFP_FAIL;
		}
	}

	return ret;
}

ufprog_status spi_nand_otp_locked(struct nand_chip *nand, ufprog_bool *retlocked)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);
	ufprog_status ret;
	uint8_t cr;

	STATUS_CHECK_RET(spi_nand_set_low_speed(snand));

	/* Leave OTP mode */
	spi_nand_otp_control(snand, false);

	ret = spi_nand_get_feature(snand, SPI_NAND_FEATURE_CONFIG_ADDR, &cr);
	if (ret) {
		logm_err("Failed to read configuration register\n");
		return ret;
	}

	*retlocked = !!(cr & SPI_NAND_CONFIG_OTP_LOCK);

	return UFP_OK;
}

const struct nand_flash_otp_ops spi_nand_otp_ops = {
	.read = spi_nand_otp_read,
	.write = spi_nand_otp_write,
	.lock = spi_nand_otp_lock,
	.locked = spi_nand_otp_locked,
};

ufprog_status spi_nand_otp_micron_lock(struct nand_chip *nand)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);

	return spi_nand_micron_enable_cfg(snand, SPI_NAND_MICRON_CR_CFG_OTP_PROTECT);
}

ufprog_status spi_nand_otp_micron_locked(struct nand_chip *nand, ufprog_bool *retlocked)
{
	struct spi_nand *snand = container_of(nand, struct spi_nand, nand);
	ufprog_status ret;
	bool locked;

	ret = spi_nand_micron_cfg_enabled(snand, SPI_NAND_MICRON_CR_CFG_OTP_PROTECT, nand->maux.oob_page_size,
					  snand->scratch_buffer, false, &locked);

	/* Restore bus speed */
	spi_nand_set_high_speed(snand);

	*retlocked = locked;

	return ret;
}

const struct nand_flash_otp_ops spi_nand_otp_micron_ops = {
	.read = spi_nand_otp_read,
	.write = spi_nand_otp_write,
	.lock = spi_nand_otp_micron_lock,
	.locked = spi_nand_otp_micron_locked,
};
