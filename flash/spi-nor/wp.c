// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash write protection operations
 */

#include <malloc.h>
#include <string.h>
#include <ufprog/log.h>
#include <ufprog/sizes.h>
#include <ufprog/spi-nor-opcode.h>
#include "core.h"
#include "regs.h"
#include "wp.h"

/* Winbond bits (<= 128Mb) */
#define SR_TB					BIT(5)
#define SR_SEC					BIT(6)
#define SR_CMP					BIT(14)

/* >= 256Mbit */
#define SR_BP3					BIT(5)
#define SR_TB4					BIT(6)

/* Masks */
#define BP_1_0					(SR_BP1 | SR_BP0)
#define BP_1_0_TB				(SR_TB | SR_BP1 | SR_BP0)
#define BP_2_0					(SR_BP2 | SR_BP1 | SR_BP0)
#define BP_2_0_TB				(SR_TB | SR_BP2 | SR_BP1 | SR_BP0)
#define BP_2_0_TB_SEC				(SR_SEC | SR_TB | SR_BP2 | SR_BP1 | SR_BP0)
#define BP_2_0_TB_SEC_CMP			(SR_CMP | SR_SEC | SR_TB | SR_BP2 | SR_BP1 | SR_BP0)
#define BP_3_0					(SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)
#define BP_3_0_TB				(SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)
#define BP_3_0_TB_CMP				(SR_CMP | SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)

const struct spi_nor_wp_info wpr_1bp = SNOR_WP_BP(&sr_acc, SR_BP0,
	SNOR_WP_NONE(0     ),	/* None */
	SNOR_WP_ALL( SR_BP0),	/* All */
);

const struct spi_nor_wp_info wpr_2bp_all = SNOR_WP_BP(&sr_acc, BP_1_0,
	SNOR_WP_NONE( 0              ),	/* None */
	SNOR_WP_ALL(  SR_BP1 | SR_BP0),	/* All */
	SNOR_WP_ALL(           SR_BP0),	/* All */
	SNOR_WP_ALL(  SR_BP1         ),	/* All */
);

const struct spi_nor_wp_info wpr_2bp_up = SNOR_WP_BP(&sr_acc, BP_1_0,
	SNOR_WP_NONE( 0                 ),	/* None */
	SNOR_WP_ALL(  SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(         SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(SR_BP1         , 1),	/* Upper 128KB */
);

const struct spi_nor_wp_info wpr_2bp_up_ratio = SNOR_WP_BP(&sr_acc, BP_1_0,
	SNOR_WP_NONE( 0                 ),	/* None */
	SNOR_WP_ALL(  SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_UP(         SR_BP0, 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(SR_BP1         , 1),	/* Upper 1/2 */
);

const struct spi_nor_wp_info wpr_2bp_lo = SNOR_WP_BP(&sr_acc, BP_1_0,
	SNOR_WP_NONE (0                 ),	/* None */
	SNOR_WP_ALL(  SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_LO(         SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(SR_BP1         , 1),	/* Lower 128KB */
);

const struct spi_nor_wp_info wpr_2bp_lo_ratio = SNOR_WP_BP(&sr_acc, BP_1_0,
	SNOR_WP_NONE (0                 ),	/* None */
	SNOR_WP_ALL(  SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_LO(         SR_BP0, 2),	/* Lower 1/4 */
	SNOR_WP_RP_LO(SR_BP1         , 1),	/* Lower 1/2 */
);

const struct spi_nor_wp_info wpr_2bp_tb = SNOR_WP_BP(&sr_acc, BP_1_0_TB,
	SNOR_WP_NONE( 0                         ),	/* None */
	SNOR_WP_NONE( SR_TB                     ),	/* None */

	SNOR_WP_ALL(          SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_TB | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(                 SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(        SR_BP1         , 1),	/* Upper 128KB */

	SNOR_WP_BP_LO(SR_TB |          SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(SR_TB | SR_BP1         , 1),	/* Lower 128KB */
);

const struct spi_nor_wp_info wpr_3bp_up = SNOR_WP_BP(&sr_acc, BP_2_0,
	SNOR_WP_NONE( 0                          ),	/* None */
	SNOR_WP_ALL(  SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(                  SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(         SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(         SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
);

const struct spi_nor_wp_info wpr_3bp_up_ratio = SNOR_WP_BP(&sr_acc, BP_2_0,
	SNOR_WP_NONE( 0                          ),	/* None */
	SNOR_WP_ALL(  SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_UP(                  SR_BP0, 6),	/* Upper 1/64 */
	SNOR_WP_RP_UP(         SR_BP1         , 5),	/* Upper 1/32 */
	SNOR_WP_RP_UP(         SR_BP1 | SR_BP0, 4),	/* Upper 1/16 */
	SNOR_WP_RP_UP(SR_BP2                  , 3),	/* Upper 1/8 */
	SNOR_WP_RP_UP(SR_BP2 |          SR_BP0, 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(SR_BP2 | SR_BP1         , 1),	/* Upper 1/2 */
);

const struct spi_nor_wp_info wpr_3bp_lo = SNOR_WP_BP(&sr_acc, BP_2_0,
	SNOR_WP_NONE( 0                          ),	/* None */
	SNOR_WP_ALL(  SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_LO(                  SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(         SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(         SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
);

const struct spi_nor_wp_info wpr_3bp_lo_ratio = SNOR_WP_BP(&sr_acc, BP_2_0,
	SNOR_WP_NONE( 0                          ),	/* None */
	SNOR_WP_ALL(  SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_LO(                  SR_BP0, 6),	/* Lower 1/64 */
	SNOR_WP_RP_LO(         SR_BP1         , 5),	/* Lower 1/32 */
	SNOR_WP_RP_LO(         SR_BP1 | SR_BP0, 4),	/* Lower 1/16 */
	SNOR_WP_RP_LO(SR_BP2                  , 3),	/* Lower 1/8 */
	SNOR_WP_RP_LO(SR_BP2 |          SR_BP0, 2),	/* Lower 1/4 */
	SNOR_WP_RP_LO(SR_BP2 | SR_BP1         , 1),	/* Lower 1/2 */
);

const struct spi_nor_wp_info wpr_3bp_tb = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_NONE( 0                                  ),	/* None */
	SNOR_WP_NONE( SR_TB                              ),	/* None */

	SNOR_WP_ALL(          SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(                          SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                 SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                 SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(        SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(        SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(        SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */

	SNOR_WP_BP_LO(SR_TB |                   SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(SR_TB |          SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(SR_TB |          SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(SR_TB | SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(SR_TB | SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(SR_TB | SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
);

const struct spi_nor_wp_info wpr_3bp_tb_ratio = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_NONE( 0                                  ),	/* None */
	SNOR_WP_NONE( SR_TB                              ),	/* None */

	SNOR_WP_ALL(          SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_UP(                          SR_BP0, 6),	/* Upper 1/64 */
	SNOR_WP_RP_UP(                 SR_BP1         , 5),	/* Upper 1/32 */
	SNOR_WP_RP_UP(                 SR_BP1 | SR_BP0, 4),	/* Upper 1/16 */
	SNOR_WP_RP_UP(        SR_BP2                  , 3),	/* Upper 1/8 */
	SNOR_WP_RP_UP(        SR_BP2 |          SR_BP0, 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(        SR_BP2 | SR_BP1         , 1),	/* Upper 1/2 */

	SNOR_WP_RP_LO(SR_TB |                   SR_BP0, 6),	/* Lower 1/64 */
	SNOR_WP_RP_LO(SR_TB |          SR_BP1         , 5),	/* Lower 1/32 */
	SNOR_WP_RP_LO(SR_TB |          SR_BP1 | SR_BP0, 4),	/* Lower 1/16 */
	SNOR_WP_RP_LO(SR_TB | SR_BP2                  , 3),	/* Lower 1/8 */
	SNOR_WP_RP_LO(SR_TB | SR_BP2 |          SR_BP0, 2),	/* Lower 1/4 */
	SNOR_WP_RP_LO(SR_TB | SR_BP2 | SR_BP1         , 1),	/* Lower 1/2 */
);

const struct spi_nor_wp_info wpr_3bp_tb_sec = SNOR_WP_BP(&sr_acc, BP_2_0_TB_SEC,
	SNOR_WP_NONE( 0                                           ),	/* None */
	SNOR_WP_NONE(          SR_TB                              ),	/* None */
	SNOR_WP_NONE( SR_SEC                                      ),	/* None */
	SNOR_WP_NONE( SR_SEC | SR_TB                              ),	/* None */

	SNOR_WP_ALL(                   SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(           SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_SEC |         SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_SEC | SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(                                   SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                          SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                          SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(                 SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(                 SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(                 SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */

	SNOR_WP_BP_LO(         SR_TB |                   SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(         SR_TB |          SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(         SR_TB |          SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(         SR_TB | SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(         SR_TB | SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(         SR_TB | SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */

	SNOR_WP_SP_UP(SR_SEC |                           SR_BP0, 0),	/* Upper 4KB */
	SNOR_WP_SP_UP(SR_SEC |                  SR_BP1         , 1),	/* Upper 8KB */
	SNOR_WP_SP_UP(SR_SEC |                  SR_BP1 | SR_BP0, 2),	/* Upper 16KB */
	SNOR_WP_SP_UP(SR_SEC |         SR_BP2                  , 3),	/* Upper 32KB */
	SNOR_WP_SP_UP(SR_SEC |         SR_BP2 |          SR_BP0, 3),	/* Upper 32KB */
	SNOR_WP_SP_UP(SR_SEC |         SR_BP2 | SR_BP1         , 3),	/* Upper 32KB */

	SNOR_WP_SP_LO(SR_SEC | SR_TB |                   SR_BP0, 0),	/* Lower 4KB */
	SNOR_WP_SP_LO(SR_SEC | SR_TB |          SR_BP1         , 1),	/* Lower 8KB */
	SNOR_WP_SP_LO(SR_SEC | SR_TB |          SR_BP1 | SR_BP0, 2),	/* Lower 16KB */
	SNOR_WP_SP_LO(SR_SEC | SR_TB | SR_BP2                  , 3),	/* Lower 32KB */
	SNOR_WP_SP_LO(SR_SEC | SR_TB | SR_BP2 |          SR_BP0, 3),	/* Lower 32KB */
	SNOR_WP_SP_LO(SR_SEC | SR_TB | SR_BP2 | SR_BP1         , 3),	/* Lower 32KB */
);

const struct spi_nor_wp_info wpr_3bp_tb_sec_ratio = SNOR_WP_BP(&sr_acc, BP_2_0_TB_SEC,
	SNOR_WP_NONE( 0                                           ),	/* None */
	SNOR_WP_NONE(          SR_TB                              ),	/* None */
	SNOR_WP_NONE( SR_SEC                                      ),	/* None */
	SNOR_WP_NONE( SR_SEC | SR_TB                              ),	/* None */

	SNOR_WP_ALL(                   SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(           SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_SEC |         SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_SEC | SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_UP(                                   SR_BP0, 6),	/* Upper 1/64 */
	SNOR_WP_RP_UP(                          SR_BP1         , 5),	/* Upper 1/32 */
	SNOR_WP_RP_UP(                          SR_BP1 | SR_BP0, 4),	/* Upper 1/16 */
	SNOR_WP_RP_UP(                 SR_BP2                  , 3),	/* Upper 1/8 */
	SNOR_WP_RP_UP(                 SR_BP2 |          SR_BP0, 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(                 SR_BP2 | SR_BP1         , 1),	/* Upper 1/2 */

	SNOR_WP_RP_LO(         SR_TB |                   SR_BP0, 6),	/* Lower 1/64 */
	SNOR_WP_RP_LO(         SR_TB |          SR_BP1         , 5),	/* Lower 1/32 */
	SNOR_WP_RP_LO(         SR_TB |          SR_BP1 | SR_BP0, 4),	/* Lower 1/16 */
	SNOR_WP_RP_LO(         SR_TB | SR_BP2                  , 3),	/* Lower 1/8 */
	SNOR_WP_RP_LO(         SR_TB | SR_BP2 |          SR_BP0, 2),	/* Lower 1/4 */
	SNOR_WP_RP_LO(         SR_TB | SR_BP2 | SR_BP1         , 1),	/* Lower 1/2 */

	SNOR_WP_SP_UP(SR_SEC |                           SR_BP0, 0),	/* Upper 4KB */
	SNOR_WP_SP_UP(SR_SEC |                  SR_BP1         , 1),	/* Upper 8KB */
	SNOR_WP_SP_UP(SR_SEC |                  SR_BP1 | SR_BP0, 2),	/* Upper 16KB */
	SNOR_WP_SP_UP(SR_SEC |         SR_BP2                  , 3),	/* Upper 32KB */
	SNOR_WP_SP_UP(SR_SEC |         SR_BP2 |          SR_BP0, 3),	/* Upper 32KB */
	SNOR_WP_SP_UP(SR_SEC |         SR_BP2 | SR_BP1         , 3),	/* Upper 32KB */

	SNOR_WP_SP_LO(SR_SEC | SR_TB |                   SR_BP0, 0),	/* Lower 4KB */
	SNOR_WP_SP_LO(SR_SEC | SR_TB |          SR_BP1         , 1),	/* Lower 8KB */
	SNOR_WP_SP_LO(SR_SEC | SR_TB |          SR_BP1 | SR_BP0, 2),	/* Lower 16KB */
	SNOR_WP_SP_LO(SR_SEC | SR_TB | SR_BP2                  , 3),	/* Lower 32KB */
	SNOR_WP_SP_LO(SR_SEC | SR_TB | SR_BP2 |          SR_BP0, 3),	/* Lower 32KB */
	SNOR_WP_SP_LO(SR_SEC | SR_TB | SR_BP2 | SR_BP1         , 3),	/* Lower 32KB */
);

const struct spi_nor_wp_info wpr_3bp_tb_sec_cmp = SNOR_WP_BP(&srcr_acc, BP_2_0_TB_SEC_CMP,
	SNOR_WP_NONE( 0                                                        ),	/* None */
	SNOR_WP_NONE(                       SR_TB                              ),	/* None */
	SNOR_WP_NONE(              SR_SEC                                      ),	/* None */
	SNOR_WP_NONE(              SR_SEC | SR_TB                              ),	/* None */
	SNOR_WP_NONE(     SR_CMP |                  SR_BP2 | SR_BP1 | SR_BP0   ),	/* None */
	SNOR_WP_NONE(     SR_CMP |          SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* None */
	SNOR_WP_NONE(     SR_CMP | SR_SEC |         SR_BP2 | SR_BP1 | SR_BP0   ),	/* None */
	SNOR_WP_NONE(     SR_CMP | SR_SEC | SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* None */

	SNOR_WP_ALL(                                SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(               SR_SEC | SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(               SR_SEC |         SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(                        SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_CMP                                               ),	/* All */
	SNOR_WP_ALL(      SR_CMP |          SR_TB                              ),	/* All */
	SNOR_WP_ALL(      SR_CMP | SR_SEC                                      ),	/* All */
	SNOR_WP_ALL(      SR_CMP | SR_SEC | SR_TB                              ),	/* All */

	SNOR_WP_BP_UP(                                                SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                                       SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                                       SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(                              SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(                              SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(                              SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */

	SNOR_WP_BP_LO(                      SR_TB |                   SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(                      SR_TB |          SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(                      SR_TB |          SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(                      SR_TB | SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(                      SR_TB | SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(                      SR_TB | SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */

	SNOR_WP_SP_UP(             SR_SEC |                           SR_BP0, 0),	/* Upper 4KB */
	SNOR_WP_SP_UP(             SR_SEC |                  SR_BP1         , 1),	/* Upper 8KB */
	SNOR_WP_SP_UP(             SR_SEC |                  SR_BP1 | SR_BP0, 2),	/* Upper 16KB */
	SNOR_WP_SP_UP(             SR_SEC |         SR_BP2                  , 3),	/* Upper 32KB */
	SNOR_WP_SP_UP(             SR_SEC |         SR_BP2 |          SR_BP0, 3),	/* Upper 32KB */
	SNOR_WP_SP_UP(             SR_SEC |         SR_BP2 | SR_BP1         , 3),	/* Upper 32KB */

	SNOR_WP_SP_LO(             SR_SEC | SR_TB |                   SR_BP0, 0),	/* Lower 4KB */
	SNOR_WP_SP_LO(             SR_SEC | SR_TB |          SR_BP1         , 1),	/* Lower 8KB */
	SNOR_WP_SP_LO(             SR_SEC | SR_TB |          SR_BP1 | SR_BP0, 2),	/* Lower 16KB */
	SNOR_WP_SP_LO(             SR_SEC | SR_TB | SR_BP2                  , 3),	/* Lower 32KB */
	SNOR_WP_SP_LO(             SR_SEC | SR_TB | SR_BP2 |          SR_BP0, 3),	/* Lower 32KB */
	SNOR_WP_SP_LO(             SR_SEC | SR_TB | SR_BP2 | SR_BP1         , 3),	/* Lower 32KB */

	SNOR_WP_BP_CMP_LO(SR_CMP |                                    SR_BP0, 0),	/* Lower T - 64KB */
	SNOR_WP_BP_CMP_LO(SR_CMP |                           SR_BP1         , 1),	/* Lower T - 128KB */
	SNOR_WP_BP_CMP_LO(SR_CMP |                           SR_BP1 | SR_BP0, 2),	/* Lower T - 256KB */
	SNOR_WP_BP_CMP_LO(SR_CMP |                  SR_BP2                  , 3),	/* Lower T - 512KB */
	SNOR_WP_BP_CMP_LO(SR_CMP |                  SR_BP2 |          SR_BP0, 4),	/* Lower T - 1MB */

	SNOR_WP_BP_CMP_UP(SR_CMP |          SR_TB |                   SR_BP0, 0),	/* Upper T - 64KB */
	SNOR_WP_BP_CMP_UP(SR_CMP |          SR_TB |          SR_BP1         , 1),	/* Upper T - 128KB */
	SNOR_WP_BP_CMP_UP(SR_CMP |          SR_TB |          SR_BP1 | SR_BP0, 2),	/* Upper T - 256KB */
	SNOR_WP_BP_CMP_UP(SR_CMP |          SR_TB | SR_BP2                  , 3),	/* Upper T - 512KB */
	SNOR_WP_BP_CMP_UP(SR_CMP |          SR_TB | SR_BP2 |          SR_BP0, 4),	/* Upper T - 1MB */

	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |                           SR_BP0, 0),	/* Lower T - 4KB */
	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |                  SR_BP1         , 1),	/* Lower T - 8KB */
	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |                  SR_BP1 | SR_BP0, 2),	/* Lower T - 16KB */
	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |         SR_BP2                  , 3),	/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |         SR_BP2 |          SR_BP0, 3),	/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |         SR_BP2 | SR_BP1         , 3),	/* Lower T - 32KB */

	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB |                   SR_BP0, 0),	/* Upper T - 4KB */
	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB |          SR_BP1         , 1),	/* Upper T - 8KB */
	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB |          SR_BP1 | SR_BP0, 2),	/* Upper T - 16KB */
	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB | SR_BP2                  , 3),	/* Upper T - 32KB */
	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB | SR_BP2 |          SR_BP0, 3),	/* Upper T - 32KB */
	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB | SR_BP2 | SR_BP1         , 3),	/* Upper T - 32KB */
);

const struct spi_nor_wp_info wpr_3bp_tb_sec_cmp_ratio = SNOR_WP_BP(&srcr_acc, BP_2_0_TB_SEC_CMP,
	SNOR_WP_NONE( 0                                                        ),	/* None */
	SNOR_WP_NONE(                       SR_TB                              ),	/* None */
	SNOR_WP_NONE(              SR_SEC                                      ),	/* None */
	SNOR_WP_NONE(              SR_SEC | SR_TB                              ),	/* None */
	SNOR_WP_NONE(     SR_CMP |                  SR_BP2 | SR_BP1 | SR_BP0   ),	/* None */
	SNOR_WP_NONE(     SR_CMP |          SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* None */
	SNOR_WP_NONE(     SR_CMP | SR_SEC |         SR_BP2 | SR_BP1 | SR_BP0   ),	/* None */
	SNOR_WP_NONE(     SR_CMP | SR_SEC | SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* None */

	SNOR_WP_ALL(                                SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(               SR_SEC | SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(               SR_SEC |         SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(                        SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_CMP                                               ),	/* All */
	SNOR_WP_ALL(      SR_CMP |          SR_TB                              ),	/* All */
	SNOR_WP_ALL(      SR_CMP | SR_SEC                                      ),	/* All */
	SNOR_WP_ALL(      SR_CMP | SR_SEC | SR_TB                              ),	/* All */

	SNOR_WP_RP_UP(                                                SR_BP0, 6),	/* Upper 1/64 */
	SNOR_WP_RP_UP(                                       SR_BP1         , 5),	/* Upper 1/32 */
	SNOR_WP_RP_UP(                                       SR_BP1 | SR_BP0, 4),	/* Upper 1/16 */
	SNOR_WP_RP_UP(                              SR_BP2                  , 3),	/* Upper 1/8 */
	SNOR_WP_RP_UP(                              SR_BP2 |          SR_BP0, 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(                              SR_BP2 | SR_BP1         , 1),	/* Upper 1/2 */

	SNOR_WP_RP_LO(                      SR_TB |                   SR_BP0, 6),	/* Lower 1/64 */
	SNOR_WP_RP_LO(                      SR_TB |          SR_BP1         , 5),	/* Lower 1/32 */
	SNOR_WP_RP_LO(                      SR_TB |          SR_BP1 | SR_BP0, 4),	/* Lower 1/16 */
	SNOR_WP_RP_LO(                      SR_TB | SR_BP2                  , 3),	/* Lower 1/8 */
	SNOR_WP_RP_LO(                      SR_TB | SR_BP2 |          SR_BP0, 2),	/* Lower 1/4 */
	SNOR_WP_RP_LO(                      SR_TB | SR_BP2 | SR_BP1         , 1),	/* Lower 1/2 */

	SNOR_WP_SP_UP(             SR_SEC |                           SR_BP0, 0),	/* Upper 4KB */
	SNOR_WP_SP_UP(             SR_SEC |                  SR_BP1         , 1),	/* Upper 8KB */
	SNOR_WP_SP_UP(             SR_SEC |                  SR_BP1 | SR_BP0, 2),	/* Upper 16KB */
	SNOR_WP_SP_UP(             SR_SEC |         SR_BP2                  , 3),	/* Upper 32KB */
	SNOR_WP_SP_UP(             SR_SEC |         SR_BP2 |          SR_BP0, 3),	/* Upper 32KB */
	SNOR_WP_SP_UP(             SR_SEC |         SR_BP2 | SR_BP1         , 3),	/* Upper 32KB */

	SNOR_WP_SP_LO(             SR_SEC | SR_TB |                   SR_BP0, 0),	/* Lower 4KB */
	SNOR_WP_SP_LO(             SR_SEC | SR_TB |          SR_BP1         , 1),	/* Lower 8KB */
	SNOR_WP_SP_LO(             SR_SEC | SR_TB |          SR_BP1 | SR_BP0, 2),	/* Lower 16KB */
	SNOR_WP_SP_LO(             SR_SEC | SR_TB | SR_BP2                  , 3),	/* Lower 32KB */
	SNOR_WP_SP_LO(             SR_SEC | SR_TB | SR_BP2 |          SR_BP0, 3),	/* Lower 32KB */
	SNOR_WP_SP_LO(             SR_SEC | SR_TB | SR_BP2 | SR_BP1         , 3),	/* Lower 32KB */

	SNOR_WP_RP_CMP_LO(SR_CMP |                                    SR_BP0, 6),	/* Lower T - 1/64 */
	SNOR_WP_RP_CMP_LO(SR_CMP |                           SR_BP1         , 5),	/* Lower T - 1/32 */
	SNOR_WP_RP_CMP_LO(SR_CMP |                           SR_BP1 | SR_BP0, 4),	/* Lower T - 1/16 */
	SNOR_WP_RP_CMP_LO(SR_CMP |                  SR_BP2                  , 3),	/* Lower T - 1/8 */
	SNOR_WP_RP_CMP_LO(SR_CMP |                  SR_BP2 |          SR_BP0, 2),	/* Lower T - 1/4 */

	SNOR_WP_RP_CMP_UP(SR_CMP |          SR_TB |                   SR_BP0, 6),	/* Upper T - 1/64 */
	SNOR_WP_RP_CMP_UP(SR_CMP |          SR_TB |          SR_BP1         , 5),	/* Upper T - 1/32 */
	SNOR_WP_RP_CMP_UP(SR_CMP |          SR_TB |          SR_BP1 | SR_BP0, 4),	/* Upper T - 1/16 */
	SNOR_WP_RP_CMP_UP(SR_CMP |          SR_TB | SR_BP2                  , 3),	/* Upper T - 1/8 */
	SNOR_WP_RP_CMP_UP(SR_CMP |          SR_TB | SR_BP2 |          SR_BP0, 2),	/* Upper T - 1/4 */

	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |                           SR_BP0, 0),	/* Lower T - 4KB */
	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |                  SR_BP1         , 1),	/* Lower T - 8KB */
	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |                  SR_BP1 | SR_BP0, 2),	/* Lower T - 16KB */
	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |         SR_BP2                  , 3),	/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |         SR_BP2 |          SR_BP0, 3),	/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(SR_CMP | SR_SEC |         SR_BP2 | SR_BP1         , 3),	/* Lower T - 32KB */

	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB |                   SR_BP0, 0),	/* Upper T - 4KB */
	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB |          SR_BP1         , 1),	/* Upper T - 8KB */
	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB |          SR_BP1 | SR_BP0, 2),	/* Upper T - 16KB */
	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB | SR_BP2                  , 3),	/* Upper T - 32KB */
	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB | SR_BP2 |          SR_BP0, 3),	/* Upper T - 32KB */
	SNOR_WP_SP_CMP_UP(SR_CMP | SR_SEC | SR_TB | SR_BP2 | SR_BP1         , 3),	/* Upper T - 32KB */
);

const struct spi_nor_wp_info wpr_4bp_up = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE( 0                                   ),	/* None */
	SNOR_WP_ALL(  SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(                           SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                  SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                  SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(         SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(         SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(         SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(         SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(SR_BP3 |                   SR_BP0, 8),	/* Upper 16MB */
	SNOR_WP_BP_UP(SR_BP3 |          SR_BP1         , 9),	/* Upper 32MB */
	SNOR_WP_BP_UP(SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Upper 64MB */
	SNOR_WP_BP_UP(SR_BP3 | SR_BP2                  , 11),	/* Upper 128MB */
	SNOR_WP_BP_UP(SR_BP3 | SR_BP2 |          SR_BP0, 12),	/* Upper 256MB */
	SNOR_WP_BP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Upper 512MB */
);

const struct spi_nor_wp_info wpr_4bp_lo = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE( 0                                   ),	/* None */
	SNOR_WP_ALL(  SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_LO(                           SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(                  SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(                  SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(         SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(         SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(         SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(         SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_LO(SR_BP3                           , 7),	/* Lower 8MB */
	SNOR_WP_BP_LO(SR_BP3 |                   SR_BP0, 8),	/* Lower 16MB */
	SNOR_WP_BP_LO(SR_BP3 |          SR_BP1         , 9),	/* Lower 32MB */
	SNOR_WP_BP_LO(SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Lower 64MB */
	SNOR_WP_BP_LO(SR_BP3 | SR_BP2                  , 11),	/* Lower 128MB */
	SNOR_WP_BP_LO(SR_BP3 | SR_BP2 |          SR_BP0, 12),	/* Lower 256MB */
	SNOR_WP_BP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Lower 512MB */
);

const struct spi_nor_wp_info wpr_4bp_tb = SNOR_WP_BP(&sr_acc, BP_3_0_TB,
	SNOR_WP_NONE( 0                                            ),	/* None */
	SNOR_WP_NONE( SR_TB4                                       ),	/* None */

	SNOR_WP_ALL(           SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(                                    SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                           SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                           SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(                  SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(                  SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(                  SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(                  SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(         SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(         SR_BP3 |                   SR_BP0, 8),	/* Upper 16MB */
	SNOR_WP_BP_UP(         SR_BP3 |          SR_BP1         , 9),	/* Upper 32MB */
	SNOR_WP_BP_UP(         SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Upper 64MB */
	SNOR_WP_BP_UP(         SR_BP3 | SR_BP2                  , 11),	/* Upper 128MB */
	SNOR_WP_BP_UP(         SR_BP3 | SR_BP2 | SR_BP1         , 12),	/* Upper 256MB */
	SNOR_WP_BP_UP(         SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Upper 512MB */

	SNOR_WP_BP_LO(SR_TB4 |                            SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(SR_TB4 |                   SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(SR_TB4 |                   SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(SR_TB4 |          SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(SR_TB4 |          SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(SR_TB4 |          SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(SR_TB4 |          SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_LO(SR_TB4 | SR_BP3                           , 7),	/* Lower 8MB */
	SNOR_WP_BP_LO(SR_TB4 | SR_BP3 |                   SR_BP0, 8),	/* Lower 16MB */
	SNOR_WP_BP_LO(SR_TB4 | SR_BP3 |          SR_BP1         , 9),	/* Lower 32MB */
	SNOR_WP_BP_LO(SR_TB4 | SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Lower 64MB */
	SNOR_WP_BP_LO(SR_TB4 | SR_BP3 | SR_BP2                  , 11),	/* Lower 128MB */
	SNOR_WP_BP_LO(SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1         , 12),	/* Lower 256MB */
	SNOR_WP_BP_LO(SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Lower 512MB */
);

const struct spi_nor_wp_info wpr_4bp_tb_cmp = SNOR_WP_BP(&srcr_acc, BP_3_0_TB_CMP,
	SNOR_WP_NONE( 0                                                         ),	/* None */
	SNOR_WP_NONE(              SR_TB4                                       ),	/* None */
	SNOR_WP_NONE(     SR_CMP |          SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* None */
	SNOR_WP_NONE(     SR_CMP | SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* None */

	SNOR_WP_ALL(                        SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(               SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_CMP                                                ),	/* All */
	SNOR_WP_ALL(      SR_CMP | SR_TB4                                       ),	/* All */

	SNOR_WP_BP_UP(                                                 SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                                        SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                                        SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(                               SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(                               SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(                               SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(                               SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(                      SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(                      SR_BP3 |                   SR_BP0, 8),	/* Upper 16MB */
	SNOR_WP_BP_UP(                      SR_BP3 |          SR_BP1         , 9),	/* Upper 32MB */
	SNOR_WP_BP_UP(                      SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Upper 64MB */
	SNOR_WP_BP_UP(                      SR_BP3 | SR_BP2                  , 11),	/* Upper 128MB */
	SNOR_WP_BP_UP(                      SR_BP3 | SR_BP2 | SR_BP1         , 12),	/* Upper 256MB */
	SNOR_WP_BP_UP(                      SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Upper 512MB */

	SNOR_WP_BP_LO(             SR_TB4 |                            SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(             SR_TB4 |                   SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(             SR_TB4 |                   SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(             SR_TB4 |          SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(             SR_TB4 |          SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(             SR_TB4 |          SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(             SR_TB4 |          SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_LO(             SR_TB4 | SR_BP3                           , 7),	/* Lower 8MB */
	SNOR_WP_BP_LO(             SR_TB4 | SR_BP3 |                   SR_BP0, 8),	/* Lower 16MB */
	SNOR_WP_BP_LO(             SR_TB4 | SR_BP3 |          SR_BP1         , 9),	/* Lower 32MB */
	SNOR_WP_BP_LO(             SR_TB4 | SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Lower 64MB */
	SNOR_WP_BP_LO(             SR_TB4 | SR_BP3 | SR_BP2                  , 11),	/* Lower 128MB */
	SNOR_WP_BP_LO(             SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1         , 12),	/* Lower 256MB */
	SNOR_WP_BP_LO(             SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Lower 512MB */

	SNOR_WP_BP_CMP_LO(SR_CMP |                                     SR_BP0, 0),	/* Lower T - 64KB */
	SNOR_WP_BP_CMP_LO(SR_CMP |                            SR_BP1         , 1),	/* Lower T - 128KB */
	SNOR_WP_BP_CMP_LO(SR_CMP |                            SR_BP1 | SR_BP0, 2),	/* Lower T - 256KB */
	SNOR_WP_BP_CMP_LO(SR_CMP |                   SR_BP2                  , 3),	/* Lower T - 512KB */
	SNOR_WP_BP_CMP_LO(SR_CMP |                   SR_BP2 |          SR_BP0, 4),	/* Lower T - 1MB */
	SNOR_WP_BP_CMP_LO(SR_CMP |                   SR_BP2 | SR_BP1         , 5),	/* Lower T - 2MB */
	SNOR_WP_BP_CMP_LO(SR_CMP |                   SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower T - 4MB */
	SNOR_WP_BP_CMP_LO(SR_CMP |          SR_BP3                           , 7),	/* Lower T - 8MB */
	SNOR_WP_BP_CMP_LO(SR_CMP |          SR_BP3 |                   SR_BP0, 8),	/* Lower T - 16MB */
	SNOR_WP_BP_CMP_LO(SR_CMP |          SR_BP3 |          SR_BP1         , 9),	/* Lower T - 32MB */
	SNOR_WP_BP_CMP_LO(SR_CMP |          SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Lower T - 64MB */
	SNOR_WP_BP_CMP_LO(SR_CMP |          SR_BP3 | SR_BP2                  , 11),	/* Lower T - 128MB */
	SNOR_WP_BP_CMP_LO(SR_CMP |          SR_BP3 | SR_BP2 | SR_BP1         , 12),	/* Lower T - 256MB */
	SNOR_WP_BP_CMP_LO(SR_CMP |          SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Lower T - 512MB */

	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 |                            SR_BP0, 0),	/* Upper T - 64KB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 |                   SR_BP1         , 1),	/* Upper T - 128KB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 |                   SR_BP1 | SR_BP0, 2),	/* Upper T - 256KB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 |          SR_BP2                  , 3),	/* Upper T - 512KB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 |          SR_BP2 |          SR_BP0, 4),	/* Upper T - 1MB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 |          SR_BP2 | SR_BP1         , 5),	/* Upper T - 2MB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 |          SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper T - 4MB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 | SR_BP3                           , 7),	/* Upper T - 8MB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 | SR_BP3 |                   SR_BP0, 8),	/* Upper T - 16MB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 | SR_BP3 |          SR_BP1         , 9),	/* Upper T - 32MB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 | SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Upper T - 64MB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 | SR_BP3 | SR_BP2                  , 11),	/* Upper T - 128MB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1         , 12),	/* Upper T - 256MB */
	SNOR_WP_BP_CMP_UP(SR_CMP | SR_TB4 | SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Upper T - 512MB */
);

struct spi_nor_wp_info *wp_bp_info_copy(const struct spi_nor_wp_info *src)
{
	struct spi_nor_wp_info *wpr;
	size_t len;

	len = sizeof(struct spi_nor_wp_info) + src->num * sizeof(struct spi_nor_wp_range);
	wpr = malloc(len);
	if (!wpr)
		return NULL;

	memcpy(wpr, src, len);

	return wpr;
}

static ufprog_status spi_nor_gen_wp_region(struct spi_nor *snor, const struct spi_nor_wp_range *range,
					   struct spi_nor_wp_region *retregion)
{
	uint64_t size;

	switch (range->type) {
	case SNOR_WPR_NONE:
		size = 0;
		break;

	case SNOR_WPR_ALL:
		size = snor->param.size;
		break;

	case SNOR_WPR_LSHIFT:
		size = (uint64_t)range->granularity << range->scale;
		break;

	case SNOR_WPR_RSHIFT:
		size = snor->param.size >> range->scale;
		break;

	case SNOR_WPR_MULTI:
		size = (uint64_t)range->granularity * range->scale;
		break;

	default:
		logm_err("Invalid BP scale type %u\n", range->type);
		return UFP_UNSUPPORTED;
	}

	if (size > snor->param.size)
		size = snor->param.size;

	if (range->flags & SNOR_WPF_CMP) {
		if (range->flags & SNOR_WPF_CMP_FULL) {
			if (!size)
				size = snor->param.size;
			else if (size == snor->param.size)
				size = 0;
		}

		if (range->flags & SNOR_WPF_LOWER) {
			retregion->base = size;
			retregion->size = snor->param.size - size;
		} else {
			retregion->base = 0;
			retregion->size = snor->param.size - size;
		}
	} else {
		if (range->flags & SNOR_WPF_LOWER) {
			retregion->base = 0;
			retregion->size = size;
		} else {
			retregion->base = snor->param.size - size;
			retregion->size = size;
		}
	}

	if (retregion->base == snor->param.size)
		retregion->base = 0;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nor_get_wp_region_list(struct spi_nor *snor, struct spi_nor_wp_regions *retregions)
{
	struct spi_nor_wp_region region, last, *r;
	bool none_set = false, all_set = false;
	uint32_t i, num = 0;

	if (!snor || !retregions)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (!snor->ext_param.wp_ranges)
		return UFP_UNSUPPORTED;

	if (snor->wp_regions) {
		retregions->num = snor->wp_regions->num;
		retregions->region = snor->wp_regions->region;
		return UFP_OK;
	}

	memset(&last, 0, sizeof(last));

	for (i = 0; i < snor->ext_param.wp_ranges->num; i++) {
		STATUS_CHECK_RET(spi_nor_gen_wp_region(snor, &snor->ext_param.wp_ranges->ranges[i], &region));

		if (!region.size) {
			if (!none_set) {
				none_set = true;
				num++;
			}
		} else if (region.size == snor->param.size) {
			if (!all_set) {
				all_set = true;
				num++;
			}
		} else {
			if (!i || (region.base != last.base || region.size != last.size))
				num++;
		}

		memcpy(&last, &region, sizeof(region));
	}

	snor->wp_regions = malloc(sizeof(*snor->wp_regions) + num * sizeof(*snor->wp_regions->region));
	if (!snor->wp_regions) {
		logm_err("No memory for BP-based write-protect regions\n");
		return UFP_NOMEM;
	}

	r = (void *)((uintptr_t)snor->wp_regions + sizeof(*snor->wp_regions));
	none_set = all_set = false;

	snor->wp_regions->num = num;
	snor->wp_regions->region = r;

	memset(&last, 0, sizeof(last));

	for (i = 0; i < snor->ext_param.wp_ranges->num; i++) {
		STATUS_CHECK_RET(spi_nor_gen_wp_region(snor, &snor->ext_param.wp_ranges->ranges[i], &region));

		if (!region.size) {
			if (!none_set) {
				none_set = true;
				*r++ = region;
			}
		} else if (region.size == snor->param.size) {
			if (!all_set) {
				all_set = true;
				*r++ = region;
			}
		} else {
			if (!i || (region.base != last.base || region.size != last.size))
				*r++ = region;
		}

		memcpy(&last, &region, sizeof(region));
	}

	retregions->num = snor->wp_regions->num;
	retregions->region = snor->wp_regions->region;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nor_get_wp_region(struct spi_nor *snor, struct spi_nor_wp_region *retregion)
{
	uint32_t i, regval;

	if (!snor || !retregion)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (!snor->ext_param.wp_ranges)
		return UFP_UNSUPPORTED;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(ufprog_spi_nor_read_reg(snor, snor->ext_param.wp_ranges->access, &regval));

	for (i = 0; i < snor->ext_param.wp_ranges->num; i++) {
		if ((regval & snor->ext_param.wp_ranges->sr_mask) !=
		    snor->ext_param.wp_ranges->ranges[i].sr_val)
			continue;

		return spi_nor_gen_wp_region(snor, &snor->ext_param.wp_ranges->ranges[i], retregion);
	}

	return UFP_FAIL;
}

ufprog_status UFPROG_API ufprog_spi_nor_set_wp_region(struct spi_nor *snor, const struct spi_nor_wp_region *region)
{
	const struct spi_nor_reg_access *access;
	struct spi_nor_wp_region rg;
	uint32_t i, regval, retval;
	ufprog_status ret;
	uint64_t base;

	if (!snor || !region)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (!snor->ext_param.wp_ranges)
		return UFP_UNSUPPORTED;

	if (region->size == snor->param.size)
		base = 0;
	else
		base = region->base;

	for (i = 0; i < snor->ext_param.wp_ranges->num; i++) {
		STATUS_CHECK_RET(spi_nor_gen_wp_region(snor, &snor->ext_param.wp_ranges->ranges[i], &rg));

		if (rg.base != base || rg.size != region->size)
			continue;

		if (snor->ext_param.wp_regacc)
			access = snor->ext_param.wp_regacc;
		else
			access = snor->ext_param.wp_ranges->access;

		STATUS_CHECK_RET(spi_nor_set_low_speed(snor));

		ufprog_spi_nor_bus_lock(snor);

		/* Set BP bits */
		STATUS_CHECK_GOTO_RET(spi_nor_read_reg_acc(snor, access, &regval), ret, out);
		regval &= ~snor->ext_param.wp_ranges->sr_mask;
		regval |= snor->ext_param.wp_ranges->ranges[i].sr_val;

		STATUS_CHECK_GOTO_RET(spi_nor_write_reg_acc(snor, access, regval, false), ret, out);

		/* Check BP bits */
		STATUS_CHECK_GOTO_RET(spi_nor_read_reg_acc(snor, access, &retval), ret, out);

		if ((retval & snor->ext_param.wp_ranges->sr_mask) ==
		    snor->ext_param.wp_ranges->ranges[i].sr_val) {
			ret = UFP_OK;
			goto out;
		}

		/* Not all bits are set correctly. Clear all bits to avoid side effects */
		regval &= ~snor->ext_param.wp_ranges->sr_mask;

		STATUS_CHECK_GOTO_RET(spi_nor_write_reg_acc(snor, access, regval, false), ret, out);

	out:
		ufprog_spi_nor_bus_unlock(snor);

		return ret;
	}

	return UFP_NOT_EXIST;
}
