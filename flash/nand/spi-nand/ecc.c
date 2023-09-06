// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash ECC status reading operations
 */

#include <string.h>
#include "core.h"
#include "ecc.h"

const struct nand_page_layout ecc_2k_64_1bit_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(8),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(8),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(8),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(8),
);

ufprog_status spi_nand_check_dummy(struct spi_nand *snand)
{
	spi_nand_reset_ecc_status(snand);

	return UFP_OK;
}

static ufprog_status spi_nand_check_extended_ecc_bfr(struct spi_nand *snand, uint32_t mask, uint32_t strength)
{
	bool ecc_err = false, ecc_corr = false;
	uint32_t i, bfr, rc;
	uint8_t bfr0, bfr1, bfr2, bfr3;

	spi_nand_reset_ecc_status(snand);

	snand->ecc_status->per_step = true;

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_BFR7_0_ADDR, &bfr0));
	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_BFR15_8_ADDR, &bfr1));

	bfr = ((uint32_t)bfr1 << 8) | bfr0;

	if (snand->state.ecc_steps == 8) {
		STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_BFR23_16_ADDR, &bfr2));
		STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_BFR31_24_ADDR, &bfr3));

		bfr |= ((uint32_t)bfr3 << 24) | ((uint32_t)bfr2 << 16);
	}

	for (i = 0; i < snand->state.ecc_steps; i++) {
		rc = (bfr >> (4 * i)) & mask;
		if (rc <= strength) {
			snand->ecc_status->step_bitflips[i] = rc;
			if (rc)
				ecc_corr = true;
		} else {
			snand->ecc_status->step_bitflips[i] = -1;
			ecc_err = true;
		}
	}

	if (ecc_err)
		return UFP_ECC_UNCORRECTABLE;

	if (ecc_corr)
		return UFP_ECC_CORRECTED;

	return UFP_OK;
}

ufprog_status spi_nand_check_extended_ecc_bfr_4b(struct spi_nand *snand)
{
	return spi_nand_check_extended_ecc_bfr(snand, 0x7, 4);
}

ufprog_status spi_nand_check_extended_ecc_bfr_8b(struct spi_nand *snand)
{
	return spi_nand_check_extended_ecc_bfr(snand, 0xf, 8);
}

ufprog_status spi_nand_check_ecc_1bit_per_step(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & SPI_NAND_STATUS_ECC_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	switch (sr) {
	case 0:
		return UFP_OK;

	case 1:
		snand->ecc_status->step_bitflips[0] = snand->nand.ecc_req.strength_per_step;
		return UFP_ECC_CORRECTED;

	default:
		snand->ecc_status->step_bitflips[0] = -1;
		return UFP_ECC_UNCORRECTABLE;
	}
}

ufprog_status spi_nand_check_ecc_8bits_sr_2bits(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & SPI_NAND_STATUS_ECC_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	if (!sr)
		return UFP_OK;

	if (sr == 1) {
		snand->ecc_status->step_bitflips[0] = snand->nand.ecc_req.strength_per_step - 1;
		return UFP_ECC_CORRECTED;
	}

	if (sr == 3) {
		snand->ecc_status->step_bitflips[0] = snand->nand.ecc_req.strength_per_step;
		return UFP_ECC_CORRECTED;
	}

	snand->ecc_status->step_bitflips[0] = -1;

	return UFP_ECC_UNCORRECTABLE;
}
