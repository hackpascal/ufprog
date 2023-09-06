// SPDX-License-Identifier: GPL-2.0-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash programmer main executable
 */

#include <stdlib.h>
#include <string.h>
#include <ufprog/dirs.h>
#include <ufprog/misc.h>
#include <ufprog/sizes.h>
#include <ufprog/osdef.h>
#include <ufprog/hexdump.h>
#include <ufprog/buffdiff.h>
#include <ufprog/onfi-param-page.h>
#include "ufsnand-common.h"

#define DEFAULT_UID_MAX_LEN				32
#define NAND_MAX_MAP_SIZE				(512 << 20)

struct ufsnand_otp_instance {
	struct ufnand_instance *nandinst;
	uint32_t index;
};

static struct ufsnand_options configs;
static struct ufsnand_instance snand_inst;

static const char usage[] =
	"Usage:\n"
	"    %s [dev=<dev>] [part=<partmodel>] [die=<id>] [ftl=<ftlcfg>] [bbt=<bbtcfg>]\n"
	"       [ecc=<ecccfg>] <subcommand> [option...]\n"
	"\n"
	"Global options:\n"
	"        dev  - Specify the device to be opened.\n"
	"               If not specified, last device recorded in config will be used.\n"
	"        part - Specify the part model to be used.\n"
	"               This will fail if flash ID mismatches.\n"
	"        die  - Specify the Die ID# to be operated.\n"
	"               If specified, only the selected Die will be used.\n"
	"               Only available for OTP and UID.\n"
	"               This is valid only if the flash has more than one Dies.\n"
	"        ftl  - Specify the FTL (Flash Translation Layer) algorithm for bad\n"
	"               block handling.\n"
	"               Its value can be one of the following type:\n"
	"                 none: Do not use FTL. Bad block marker is also ignored.\n"
	"                 <ftl-plugin>: Use specified FTL plugin\n"
	"                 <ftl-plugin>,<config>: Use specified FTL plugin with\n"
	"                                        configuration file\n"
	"               If not specified, default FTL algorithm will be used.\n"
	"               The default FTL simply skips bad blocks, and redo\n"
	"               read/write/erase on next good block.\n"
	"        bbt  - Specify the BBT (Bad Block Table) algorithm to be used.\n"
	"               Its value can be one of the following type:\n"
	"                 <bbt-plugin>: Use specified BBT plugin\n"
	"                 <bbt-plugin>,<config>: Use specified BBT plugin with\n"
	"                                        configuration file\n"
	"               If not specified, default RAM-based BBT will be used,\n"
	"               and it will not be written back to NAND.\n"
	"               Select BBT may not be used by FTL.\n"
	"        ecc  - Specify the ECC engine for page read/write.\n"
	"               Its value can be one of the following type:\n"
	"                 none: Do not use ECC engine\n"
	"                 <ecc-plugin>: Use specified ECC engine plugin\n"
	"                 <ecc-plugin>,<config>: Use specified ECC engine plugin with\n"
	"                                        configuration file\n"
	"               If not specified, default ECC engine provided by the spi-nand\n"
	"               controller will be used. The default ECC engine may be the\n"
	"               On-die ECC engine if supported, or 'none'.\n"
	"\n"
	"Read/write/erase common options:\n"
	"    ... [raw] [oob] [fmt] [nospread] [part-base=<base>] [part-size=<size>]\n"
	"\n"
	"        raw  - Turn off ECC engine for read/write. But page data layout\n"
	"               conversion is still available.\n"
	"        oob  - For read operation, OOB data should be also read. For write\n"
	"               operation, input file data contains OOB data and should be also\n"
	"               written.\n"
	"        fmt  - Requires page data layout conversion.\n"
	"               The page data format in which continuous user data followed by\n"
	"               OOB data is called canonical page layout is this program.\n"
	"               Generally the page data is stored in NAND data array as-is.\n"
	"               However some ECC engines use different data arrangement like\n"
	"               interleaved per-subpage user data and oob data in NAND data\n"
	"               array, but still provides re-arranged data in canonical page\n"
	"               layout to user.\n"
	"               This option idicates that the input file data is in canonical\n"
	"               page layout and must be converted to raw page layout before\n"
	"               written. The data read from NAND must also be converted to\n"
	"               canonical page layout before writting to file.\n"
	"        nospread - When default FTL is used, this option tells FTL that return\n"
	"               error immediately if write/erase operation fails. Redo on next\n"
	"               good block is not allowed.\n"
	"        part-base - Define a partition, and specify its physical base address.\n"
	"               When this option is set, the read/write/erase address becomes\n"
	"               logical address relative to the partition base address.\n"
	"               If unspecified, initial read/write/erase address will be used.\n"
	"               The part base address must be block size aligned.\n"
	"               The behavior of logical address in part depends on the selected\n"
	"               FTL.\n"
	"        part-size - Specify the size of the partition."
	"               If unspecified, the maximum size from part base will be used.\n"
	"               The part size must be block size aligned.\n"
	"\n"
	"Subcommands:\n"
	"    list vendors\n"
	"        List all vendors supported.\n"
	"    list [vendor=<vendorid>] [<match>]\n"
	"        List flash parts supported.\n"
	"        vendor - Specify the vendor ID to be listed.\n"
	"                 By default all vendors will be listed.\n"
	"                 All vendor IDs can be listed by subcommand 'list vendors'.\n"
	"        match  - Specify the (sub)string that the part model should contain.\n"
	"\n"
	"    probe\n"
	"        Detect the flash chip model and display its information.\n"
	"        Bad block will also be scanned.\n"
	"\n"
	"    bad\n"
	"        Scan bad blocks.\n"
	"\n"
	"    read [r/w/e options] <file> [<addr> [<size>|count=<n>]]\n"
	"        Read flash data to file.\n"
	"        file  - The file path used to store flash data.\n"
	"        addr  - The start flash address to read from.\n"
	"                The value of address must be page size (not including OOB)\n"
	"                aligned.\n"
	"                Default is 0 if not specified.\n"
	"        size  - The size to be read.\n"
	"                The value of size must be page size (not including OOB)\n"
	"                aligned.\n"
	"                Default is the size from start address to end of flash.\n"
	"        count - Number of pages to be read.\n"
	"\n"
	"    dump pp\n"
	"        Dump parameter page data to stdout if exists.\n"
	"    dump [r/w/e options] [<addr> [<size>|count=<n>]]\n"
	"        Dump flash data to stdout.\n"
	"        addr - The start flash address to dumped.\n"
	"               The page that the address (not including OOB) is pointed will be\n"
	"               dumped.\n"
	"               Default is 0 if not specified.\n"
	"        size  - The size to be read for dump.\n"
	"                The value of size must be page size (not including OOB)\n"
	"                aligned.\n"
	"                Default is one page.\n"
	"        count - Number of pages to be read for dump.\n"
	"\n"
	"    write [r/w/e options] [erase] [verify] <file> [<addr> [<size>|count=<n>]]\n"
	"        Write flash data from file.\n"
	"        erase  - Erase block(s) the data will be written to.\n"
	"        verify - Verify the data being written.\n"
	"        file   - The file to be written to flash.\n"
	"                 The file size must be page size (w/ or w/o OOB) aligned.\n"
	"        addr   - The start flash address to be written to.\n"
	"                 The value of address must be page size (not including OOB)\n"
	"                 aligned.\n"
	"                 Default is 0 if not specified.\n"
	"        size   - The size to be written. Default is the writable size from start\n"
	"                 address to end of flash."
	"                 The value of size must be page size (not including OOB)\n"
	"                 aligned.\n"
	"        count  - Number of pages to be written.\n"
	"\n"
	"    erase [r/w/e options] chip|[<addr> [<size>|count=<n>]]\n"
	"        Erase flash range.\n"
	"        chip  - Erase the whole chip.\n"
	"        addr  - The start flash address to be erased.\n"
	"                Default is 0 if not specified.\n"
	"        size  - The size to be erased. Default is the size from start address to\n"
	"                end of flash.\n"
	"                All blocks covered by the erase range will be erased.\n"
	"        count - Number of blocks to be erased.\n"
	"\n"
	"    markbad [<addr>]\n"
	"        Write bad block marker to block specified by <addr>.\n"
	"\n"
	"    uid\n"
	"        Read the Unique ID if supported.\n"
	"\n"
	"    otp info\n"
	"        Display OTP region information.\n"
	"    otp [index=<index>] read [raw] [oob] [fmt] <file>\n"
	"        Read OTP region into file.\n"
	"    otp [index=<index>] write [raw] [oob] [fmt] <file>\n"
	"        Write data to OTP region.\n"
	"    otp lock\n"
	"        Lock OTP region. The OTP region lock is permanent.\n"
	"        index  - Specify the OTP region index to be operated. This must be\n"
	"                 specified if more than one regions exist.\n"
	"        file   - The file to be read from/written to OTP region.\n"
	"\n"
	"    nor_read status\n"
	"        Display NOR read timing emulation status.\n"
	"    nor_read enable\n"
	"        Enable NOR read timing emulation.\n";

static void show_usage(void)
{
	os_printf(usage, os_prog_name());
}

static void nand_print_chip_info(struct ufnand_instance *nandinst)
{
	struct ufprog_nand_ecc_chip *ecc;
	struct nand_ecc_config ecccfg;

	os_printf("\n");
	os_printf("Memory organization:\n");
	os_printf("  Num of CE#:       %u\n", nandinst->info.memorg.num_chips);
	os_printf("  LUNs per CE#:     %u\n", nandinst->info.memorg.luns_per_cs);
	os_printf("  Blocks per LUN:   %u\n", nandinst->info.memorg.blocks_per_lun);
	os_printf("  Planes per LUN:   %u\n", nandinst->info.memorg.planes_per_lun);
	os_printf("  Pages per block:  %u\n", nandinst->info.memorg.pages_per_block);
	os_printf("  Page size:        %uB\n", nandinst->info.memorg.page_size);
	os_printf("  OOB size:         %uB\n", nandinst->info.memorg.oob_size);
	os_printf("\n");
	os_printf("  Block size:       %uKB\n", nandinst->info.maux.block_size >> 10);

	os_printf("\n");
	os_printf("Default ECC information:\n");

	ecc = ufprog_nand_default_ecc(nandinst->chip);
	if (ecc && ufprog_ecc_chip_type(ecc) != NAND_ECC_NONE) {
		ufprog_ecc_get_config(ecc, &ecccfg);
		os_printf("  Type:             %s\n", ufprog_ecc_chip_type_name(ecc));
		os_printf("  Capability:       %ub per %uB\n", ecccfg.strength_per_step, ecccfg.step_size);
	} else {
		os_printf("  ECC not available\n");
	}

	os_printf("  Chip requirement: %ub per %uB\n", nandinst->info.ecc_req.strength_per_step,
		  nandinst->info.ecc_req.step_size);

	if (nandinst->info.otp_pages || nandinst->info.uid_length) {
		os_printf("\n");

		if (nandinst->info.otp_pages)
			os_printf("OTP:                %u pages\n", nandinst->info.otp_pages);

		if (nandinst->info.uid_length)
			os_printf("Unique ID:          %u bytes\n", nandinst->info.uid_length);
	}
}

static void nand_print_chip_config(struct ufnand_instance *nandinst)
{
	struct ufprog_nand_ecc_chip *ecc;
	struct nand_ecc_config ecccfg;
	struct nand_bbm_config bbmcfg;
	uint32_t i;

	ecc = ufprog_nand_get_ecc(nandinst->chip);
	if (ecc != ufprog_nand_default_ecc(nandinst->chip)) {
		ufprog_ecc_get_config(ecc, &ecccfg);

		os_printf("\n");
		os_printf("Current ECC information:\n");
		os_printf("  Type:             %s\n", ufprog_ecc_chip_type_name(ecc));
		os_printf("  Capability:       %ub per %uB\n", ecccfg.strength_per_step, ecccfg.step_size);
	}

	ufprog_nand_get_bbm_config(nandinst->chip, &bbmcfg);

	os_printf("\n");
	os_printf("Bad block information:\n");
	os_printf("  Bad marker bits:  %u\n", bbmcfg.check.width);

	os_printf("  Check page:       ");

	for (i = 0; i < bbmcfg.pages.num; i++)
		os_printf("%u ", bbmcfg.pages.idx[i]);

	os_printf("\n");

	os_printf("  Check pos:        ");

	for (i = 0; i < bbmcfg.check.num; i++)
		os_printf("%u ", bbmcfg.check.pos[i]);

	os_printf("\n");

	os_printf("  Mark pos:         ");

	for (i = 0; i < bbmcfg.mark.num; i++)
		os_printf("%u ", bbmcfg.mark.pos[i]);

	os_printf("\n");
}

static ufprog_status do_nand_bad(struct ufnand_instance *nandinst)
{
	uint32_t i, count, bcnt = 0;
	uint64_t addr = 0;

	count = (uint32_t)(nandinst->ftl_size >> nandinst->info.maux.block_shift);

	os_printf("Scanning bad blocks ...\n");

	for (i = 0; i < count; i++, addr += nandinst->info.maux.block_size) {
		if (ufprog_ftl_block_checkbad(nandinst->ftl, i) == UFP_FAIL) {
			os_printf("Bad block %u at 0x%" PRIx64 "\n", i, addr);
			bcnt++;
		}
	}

	if (!bcnt)
		os_printf("No bad block found\n");

	return UFP_OK;
}

static ufprog_status nand_prepare_opdata(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata,
					 struct ufnand_op_data *opdata)
{
	struct ufprog_nand_ecc_chip *ecc;
	ufprog_status ret;

	memset(opdata, 0, sizeof(*opdata));

	ecc = ufprog_nand_get_ecc(nandinst->chip);
	if (ecc)
		opdata->layout = ufprog_ecc_get_page_layout(ecc, rwedata->fmt &&
							         ufprog_ecc_support_convert_page_layout(ecc));

	if (!opdata->layout) {
		ret = ufprog_nand_generate_page_layout(nandinst->chip, (struct nand_page_layout **)&opdata->layout);
		if (ret) {
			os_fprintf(stderr, "Failed to generate default page layout\n");
			return ret;
		}

		opdata->layout_needs_free = true;
	} else {
		opdata->layout_needs_free = false;
	}

	opdata->buf[0] = malloc(nandinst->info.maux.oob_block_size * 2 + nandinst->info.maux.oob_page_size * 2);
	if (!opdata->buf[0]) {
		os_fprintf(stderr, "No memory for R/W buffer\n");
		goto cleanup_layout;
	}

	opdata->buf[1] = opdata->buf[0] + nandinst->info.maux.oob_block_size;
	opdata->map = opdata->buf[1] + nandinst->info.maux.oob_block_size;
	opdata->tmp = opdata->map + nandinst->info.maux.oob_page_size;

	ufprog_nand_page_layout_to_map(opdata->layout, opdata->map);

	if (rwedata->oob)
		opdata->page_size = nandinst->info.maux.oob_page_size;
	else
		opdata->page_size = nandinst->info.memorg.page_size;

	return UFP_OK;

cleanup_layout:
	if (opdata->layout_needs_free)
		ufprog_nand_free_page_layout((void *)opdata->layout);

	memset(opdata, 0, sizeof(*opdata));

	return UFP_NOMEM;
}

static void nand_cleanup_opdata(struct ufnand_op_data *opdata)
{
	if (opdata->layout_needs_free)
		ufprog_nand_free_page_layout((void *)opdata->layout);

	if (opdata->buf[0])
		free(opdata->buf[0]);

	memset(opdata, 0, sizeof(*opdata));
}

static void print_part_info(struct ufnand_instance *nandinst, struct ufprog_ftl_part *part)
{
	os_printf("Defined partition: [0x%" PRIx64 " - 0x%" PRIx64 "]\n",
		  (uint64_t)part->base_block << nandinst->info.maux.block_shift,
		  (uint64_t)(part->base_block + part->block_count) << nandinst->info.maux.block_shift);
}

static ufprog_status do_nand_read(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata, uint32_t page,
				  uint32_t count, const char *file)
{
	struct ufnand_op_data opdata;
	uint64_t data_size;
	ufprog_status ret;
	file_mapping fm;

	ret = nand_prepare_opdata(nandinst, rwedata, &opdata);
	if (ret)
		return ret;

	if (!rwedata->part_set) {
		rwedata->part.base_block = page >> nandinst->info.maux.pages_per_block_shift;
		rwedata->part.block_count = (uint32_t)(nandinst->ftl_size >> nandinst->info.maux.block_shift) -
			rwedata->part.base_block;
		page &= nandinst->info.maux.pages_per_block_mask;
	}

	data_size = (uint64_t)opdata.page_size * count;

	ret = os_open_file_mapping(file, data_size, NAND_MAX_MAP_SIZE, true, true, &fm);
	if (ret)
		goto cleanup_opdata;

	if (!os_set_file_mapping_offset(fm, 0, NULL)) {
		ret = UFP_FILE_WRITE_FAILURE;
		goto cleanup;
	}

	if (rwedata->part_set)
		print_part_info(nandinst, &rwedata->part);

	ret = nand_read(nandinst, rwedata, &rwedata->part, &opdata, fm, page, count);

cleanup:
	os_close_file_mapping(fm);

cleanup_opdata:
	nand_cleanup_opdata(&opdata);

	return ret;
}

static ufprog_status do_nand_dump(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata, uint32_t page,
				  uint32_t count)
{
	struct ufnand_op_data opdata;
	ufprog_status ret;

	ret = nand_prepare_opdata(nandinst, rwedata, &opdata);
	if (ret)
		return ret;

	if (!rwedata->part_set) {
		rwedata->part.base_block = page >> nandinst->info.maux.pages_per_block_shift;
		rwedata->part.block_count = (uint32_t)(nandinst->ftl_size >> nandinst->info.maux.block_shift) -
			rwedata->part.base_block;
		page &= nandinst->info.maux.pages_per_block_mask;
	}

	if (rwedata->part_set)
		print_part_info(nandinst, &rwedata->part);

	ret = nand_dump(nandinst, rwedata, &rwedata->part, &opdata, page, count);

	nand_cleanup_opdata(&opdata);

	return ret;
}

static ufprog_status do_nand_write(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata, uint32_t page,
				   uint32_t count, const char *file)
{
	uint32_t last_page_padding = 0;
	uint64_t data_size, file_size;
	struct ufnand_op_data opdata;
	file_handle fileh;
	ufprog_status ret;
	file_mapping fm;

	ret = nand_prepare_opdata(nandinst, rwedata, &opdata);
	if (ret)
		return ret;

	if (!rwedata->part_set) {
		rwedata->part.base_block = page >> nandinst->info.maux.pages_per_block_shift;
		rwedata->part.block_count = (uint32_t)(nandinst->ftl_size >> nandinst->info.maux.block_shift) -
			rwedata->part.base_block;
		page &= nandinst->info.maux.pages_per_block_mask;
	}

	data_size = (uint64_t)opdata.page_size * count;

	ret = os_open_file_mapping(file, 0, NAND_MAX_MAP_SIZE, false, false, &fm);
	if (ret)
		goto cleanup_opdata;

	fileh = os_get_file_mapping_file_handle(fm);
	if (!os_get_file_size(fileh, &file_size)) {
		ret = UFP_FILE_READ_FAILURE;
		goto cleanup;
	}

	if (!file_size) {
		os_printf("Input file is empty.\n");
		goto cleanup;
	}

	if (file_size % opdata.page_size)
		last_page_padding = opdata.page_size - (file_size % opdata.page_size);

	if (file_size < data_size) {
		count = (uint32_t)((file_size + opdata.page_size - 1) / opdata.page_size);

		if (count > 1)
			os_fprintf(stderr, "Write size truncated to 0x%" PRIx64 " (%u pages)\n", file_size, count);
		else
			os_fprintf(stderr, "Write size truncated to 0x%" PRIx64 " (1 page)\n", file_size);

		os_fprintf(stderr, "\n");
	}

	if (!os_set_file_mapping_offset(fm, 0, NULL)) {
		ret = UFP_FILE_READ_FAILURE;
		goto cleanup;
	}

	if (rwedata->part_set)
		print_part_info(nandinst, &rwedata->part);

	if (rwedata->erase) {
		ret = nand_erase(nandinst, &rwedata->part, page, count, rwedata->nospread);
		if (ret)
			goto cleanup;

		os_printf("\n");
	}

	ret = nand_write(nandinst, rwedata, &rwedata->part, &opdata, fm, page, count, last_page_padding);
	if (ret)
		goto cleanup;

	if (rwedata->verify) {
		os_printf("\n");

		ret = nand_verify(nandinst, rwedata, &rwedata->part, &opdata, fm, page, count, last_page_padding);
		if (ret)
			goto cleanup;
	}

cleanup:
	os_close_file_mapping(fm);

cleanup_opdata:
	nand_cleanup_opdata(&opdata);

	return ret;
}

static ufprog_status do_nand_erase(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata, uint32_t page,
				   uint32_t count)
{
	if (!rwedata->part_set) {
		rwedata->part.base_block = page >> nandinst->info.maux.pages_per_block_shift;
		rwedata->part.block_count = (uint32_t)(nandinst->ftl_size >> nandinst->info.maux.block_shift) -
			rwedata->part.base_block;
		page = 0;
	}

	if (rwedata->part_set)
		print_part_info(nandinst, &rwedata->part);

	return nand_erase(nandinst, &rwedata->part, page, count, rwedata->nospread);
}

static ufprog_status do_nand_markbad(struct ufnand_instance *nandinst, uint64_t addr)
{
	uint32_t block = (uint32_t)(addr >> nandinst->info.maux.block_shift);
	ufprog_status ret;

	ret = ufprog_nand_markbad(nandinst->chip, NULL, block);
	if (ret) {
		os_fprintf(stderr, "Failed to mark bad block %u at 0x%" PRIx64 "\n", block,
			   addr & ~(uint64_t)nandinst->info.maux.block_mask);
	} else {
		os_printf("Block %u at 0x%" PRIx64 " marked as bad\n", block,
			  addr & ~(uint64_t)nandinst->info.maux.block_mask);
	}

	return ret;
}

static ufprog_status do_nand_uid(struct ufnand_instance *nandinst)
{
	uint8_t uiddfl[DEFAULT_UID_MAX_LEN], *uid = NULL;
	ufprog_status ret;
	uint32_t len, i;

	ret = ufprog_nand_read_uid(nandinst->chip, NULL, &len);
	if (ret == UFP_UNSUPPORTED) {
		os_fprintf(stderr, "Unique ID is not supported by this flash chip\n");
		return ret;
	}

	if (len <= sizeof(uiddfl))
		uid = uiddfl;

	if (!uid) {
		uid = malloc(len);
		if (!uid) {
			logm_err("No memory for Unique ID\n");
			return UFP_NOMEM;
		}
	}

	ret = ufprog_nand_read_uid(nandinst->chip, uid, NULL);
	if (ret) {
		os_fprintf(stderr, "Failed to read Unique ID\n");
		goto out;
	}

	os_printf("Unique ID: ");

	for (i = 0; i < len; i++)
		os_printf("%02x", uid[i]);

	os_printf("\n");

	ret = UFP_OK;

out:
	if (uid != uiddfl)
		free(uid);

	return ret;
}

static ufprog_status do_nand_otp_info(struct ufnand_instance *nandinst)
{
	ufprog_bool locked;
	ufprog_status ret;

	ret = ufprog_nand_otp_locked(nandinst->chip, &locked);
	if (ret) {
		os_fprintf(stderr, "Failed to get lock status of OTP\n");
		return ret;
	}

	os_printf("OTP information:\n");
	os_printf("    Page count:   %u\n", nandinst->info.otp_pages);
	os_printf("    Locked:       %s\n", locked ? "yes" : "no");

	return UFP_OK;
}

static ufprog_status do_nand_otp_read(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata, uint32_t index,
				      const char *file)
{
	uint32_t page_size;
	uint8_t *data, *tmp;
	ufprog_status ret;
	file_mapping fm;
	void *p;

	data = malloc(nandinst->info.maux.oob_page_size * 2);
	if (!data)
		return UFP_NOMEM;

	tmp = data + nandinst->info.maux.oob_page_size;

	if (rwedata->oob)
		page_size = nandinst->info.maux.oob_page_size;
	else
		page_size = nandinst->info.memorg.page_size;

	ret = ufprog_nand_otp_read(nandinst->chip, index, data, rwedata->raw);
	if (ret) {
		os_fprintf(stderr, "Failed to read OTP page %u\n", index);
		goto cleanup_data;
	}

	ret = os_open_file_mapping(file, page_size, page_size, true, true, &fm);
	if (ret)
		goto cleanup_data;

	if (!os_set_file_mapping_offset(fm, 0, &p)) {
		ret = UFP_FILE_WRITE_FAILURE;
		goto cleanup;
	}

	if (rwedata->fmt) {
		ret = ufprog_nand_convert_page_format(nandinst->chip, data, tmp, false);
		if (ret) {
			os_fprintf(stderr, "Failed to convert page data\n");
			goto cleanup;
		}

		memcpy(p, tmp, page_size);
	} else {
		memcpy(p, data, page_size);
	}

	os_printf("OTP page %u has been read to '%s'\n", index, file);

cleanup:
	os_close_file_mapping(fm);

cleanup_data:
	free(data);

	return ret;
}

static ufprog_status do_nand_otp_write(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata,
				       uint32_t index, const char *file)
{
	uint32_t page_size;
	uint64_t file_size;
	uint8_t *data, *tmp;
	file_handle fileh;
	ufprog_status ret;
	file_mapping fm;
	void *p;

	data = malloc(nandinst->info.maux.oob_page_size * 2);
	if (!data)
		return UFP_NOMEM;

	tmp = data + nandinst->info.maux.oob_page_size;

	if (rwedata->oob)
		page_size = nandinst->info.maux.oob_page_size;
	else
		page_size = nandinst->info.memorg.page_size;

	ret = os_open_file_mapping(file, 0, page_size, false, false, &fm);
	if (ret)
		goto cleanup_data;

	fileh = os_get_file_mapping_file_handle(fm);
	if (!os_get_file_size(fileh, &file_size)) {
		ret = UFP_FILE_READ_FAILURE;
		goto cleanup;
	}

	if (file_size != page_size) {
		os_fprintf(stderr, "Input file size (0x%" PRIx64 ") is not required page size (0x%x)\n",
			   file_size, page_size);
		ret = UFP_INVALID_PARAMETER;
		goto cleanup;
	}

	if (!os_set_file_mapping_offset(fm, 0, &p)) {
		ret = UFP_FILE_WRITE_FAILURE;
		goto cleanup;
	}

	if (rwedata->fmt) {
		ret = ufprog_nand_convert_page_format(nandinst->chip, p, tmp, true);
		if (ret) {
			os_fprintf(stderr, "Failed to convert page data\n");
			goto cleanup;
		}

		memcpy(data, tmp, page_size);
	} else {
		memcpy(data, p, page_size);
	}

	ret = ufprog_nand_otp_write(nandinst->chip, index, data, rwedata->raw);
	if (ret) {
		os_fprintf(stderr, "Failed to write OTP page %u\n", index);
		goto cleanup_data;
	}

	os_printf("OTP page %u has been written with data from '%s'\n", index, file);

cleanup:
	os_close_file_mapping(fm);

cleanup_data:
	free(data);

	return ret;
}

static ufprog_status do_nand_otp_lock(struct ufnand_instance *nandinst)
{
	bool confirmed = false;
	ufprog_bool locked;
	ufprog_status ret;
	char *confirm_str;

	ret = ufprog_nand_otp_locked(nandinst->chip, &locked);
	if (ret) {
		os_fprintf(stderr, "Failed to get lock status of OTP\n");
		return ret;
	}

	if (locked) {
		os_fprintf(stderr, "OTP region has already been locked\n");
		return UFP_OK;
	}

	os_printf("Locking OTP region is irreversible and its data will be read-only forever.\n");
	os_printf("Are you sure you want to lock it? Type \"confirm\" with enter to continue.\n");

	confirm_str = os_getline_alloc(stdin);
	if (!confirm_str) {
		os_fprintf(stderr, "Failed to read from stdin\n");
		return UFP_FAIL;
	}

	if (!strncmp(confirm_str, "confirm", 7))
		confirmed = true;

	free(confirm_str);

	if (!confirmed) {
		os_fprintf(stderr, "Error: OTP locking cancelled\n");
		return UFP_OK;
	}

	ret = ufprog_nand_otp_lock(nandinst->chip);
	if (ret) {
		os_fprintf(stderr, "Failed to lock OTP region\n");
		return ret;
	}

	os_printf("OTP region is permanently locked now\n");

	return UFP_OK;
}

static int do_snand_list(void *priv, int argc, char *argv[])
{
	struct spi_nand_vendor_item *vendors;
	uint32_t i, n, vendor_width = 0;
	struct spi_nand_part_list *list;
	char *vendor = NULL, *part;
	ufprog_status ret;
	int argp;

	struct cmdarg_entry args[] = {
		CMDARG_STRING_OPT("vendor", vendor),
	};

	if (argc > 1 && !strcmp(argv[1], "vendors")) {
		ret = ufprog_spi_nand_list_vendors(&vendors, &n);
		if (ret) {
			os_fprintf(stderr, "Failed to get vendor list\n");
			return 1;
		}

		os_printf("Supported vendors (ID/name):\n");

		for (i = 0; i < n; i++)
			os_printf("    %s\t%s\n", vendors[i].id, vendors[i].name);

		return 0;
	}

	if (!parse_args(args, ARRAY_SIZE(args), argc, argv, &argp))
		return 1;

	if (argp >= argc)
		part = NULL;
	else
		part = argv[argp];

	ret = ufprog_spi_nand_list_parts(&list, vendor, part);
	if (ret) {
		os_fprintf(stderr, "Failed to get flash part list\n");
		return 1;
	}

	if (!vendor && !part)
		os_printf("Supported parts:\n");
	else if (vendor && !part)
		os_printf("Supported parts from \"%s\":\n", vendor);
	else if (!vendor && part)
		os_printf("Supported parts containing \"%s\":\n", part);
	else
		os_printf("Supported parts containing \"%s\" from \"%s\":\n", part, vendor);

	/* Calculate the vendor name width */
	for (i = 0; i < list->num; i++) {
		if (strlen(list->list[i].vendor) > vendor_width)
			vendor_width = (uint32_t)strlen(list->list[i].vendor);
	}

	vendor_width = (vendor_width + 3) & ~3;

	for (i = 0; i < list->num; i++)
		os_printf("    %-*s%s\n", vendor_width, list->list[i].vendor, list->list[i].name);

	printf("Total: %u\n", list->num);

	ufprog_spi_nand_free_list(list);

	return 0;
}

static int do_snand_probe(void *priv, int argc, char *argv[])
{
	struct ufsnand_instance *inst = priv;
	char idstr[20], *model, *vendor;
	struct spi_nand_part_list *list;
	ufprog_status ret;
	uint32_t i;

	bin_to_hex_str(idstr, sizeof(idstr), inst->nand.info.id.id, inst->nand.info.id.len, true, true);
	os_printf("JEDEC ID:           %s\n", idstr);

	os_printf("Max speed:          %uMHz\n", inst->sinfo.max_speed / 1000000);
	os_printf("Read I/O:           %u-%u-%u\n", spi_mem_io_info_cmd_bw(inst->sinfo.rd_io_info),
		  spi_mem_io_info_addr_bw(inst->sinfo.rd_io_info), spi_mem_io_info_data_bw(inst->sinfo.rd_io_info));
	os_printf("Write I/O:          %u-%u-%u\n", spi_mem_io_info_cmd_bw(inst->sinfo.pl_io_info),
		  spi_mem_io_info_addr_bw(inst->sinfo.pl_io_info), spi_mem_io_info_data_bw(inst->sinfo.pl_io_info));

	nand_print_chip_info(&inst->nand);
	nand_print_chip_config(&inst->nand);

	model = os_strdup(inst->nand.info.model);
	if (!model) {
		os_fprintf(stderr, "No memory for model name\n");
		return 1;
	}

	vendor = os_strdup(inst->nand.info.vendor);
	if (!vendor) {
		os_fprintf(stderr, "No memory for vendor name\n");
		free(model);
		return 1;
	}

	ret = ufprog_spi_nand_probe(inst->snand, &list, NULL);
	if (ret)
		goto cleanup;

	if (list->num == 1)
		goto cleanup;

	os_printf("\n");
	os_printf("Other matched part(s):\n");

	for (i = 0; i < list->num; i++) {
		if (!strcasecmp(model, list->list[i].name) && !strcasecmp(vendor, list->list[i].vendor))
			continue;

		os_printf("    %s\t%s\n", list->list[i].vendor, list->list[i].name);
	}

	ufprog_spi_nand_free_list(list);

cleanup:
	free(model);
	free(vendor);

	return 0;
}

static int do_snand_bad(void *priv, int argc, char *argv[])
{
	struct ufsnand_instance *inst = priv;
	ufprog_status ret;

	ret = do_nand_bad(&inst->nand);
	if (ret)
		return 1;

	return 0;
}

static int parse_rwe_options(struct ufnand_rwe_data *rwedata, const struct nand_memaux_info *maux, uint64_t ftl_size,
			     int argc, char *argv[])
{
	uint64_t part_base = 0, part_size = 0;
	ufprog_bool part_size_set = false;
	int argp;

	struct cmdarg_entry args[] = {
		CMDARG_BOOL_OPT("raw", rwedata->raw),
		CMDARG_BOOL_OPT("oob", rwedata->oob),
		CMDARG_BOOL_OPT("fmt", rwedata->fmt),
		CMDARG_BOOL_OPT("nospread", rwedata->nospread),
		CMDARG_BOOL_OPT("verify", rwedata->verify),
		CMDARG_BOOL_OPT("erase", rwedata->erase),
		CMDARG_U64_OPT_SET("part-base", part_base, rwedata->part_set),
		CMDARG_U64_OPT_SET("part-size", part_size, part_size_set),
	};

	memset(rwedata, 0, sizeof(*rwedata));

	if (!parse_args(args, ARRAY_SIZE(args), argc, argv, &argp))
		return -1;

	if (rwedata->part_set) {
		if (part_base & maux->block_mask) {
			os_fprintf(stderr, "part-base must be aligned to block boundary\n");
			return -1;
		}

		if (part_base >= ftl_size) {
			os_fprintf(stderr, "part-base exceeds flash size\n");
			return -1;
		}

		if (part_size_set) {
			if (part_size & maux->block_mask) {
				os_fprintf(stderr, "part-size must be multiple of block size\n");
				return -1;
			}

			if (!part_size) {
				os_fprintf(stderr, "part-size must not be zero\n");
				return -1;
			}

			if (part_base + part_size > ftl_size) {
				os_fprintf(stderr, "part-size exceeds flash size\n");
				return -1;
			}
		} else {
			part_size = ftl_size - part_base;
		}
	} else {
		part_size = 0;
	}

	rwedata->part.base_block = (uint32_t)(part_base >> maux->block_shift);
	rwedata->part.block_count = (uint32_t)(part_size >> maux->block_shift);

	return argp;
}

static int parse_addr_size(struct ufnand_rwe_data *rwedata, uint32_t *retpage, uint32_t *retcount,
			   const struct nand_memaux_info *maux, bool by_block, bool for_dump, uint64_t ftl_size,
			   int argc, char *argv[])
{
	bool by_count = false;
	char *end, *sizestr;
	uint64_t addr, size;

	if (!argc) {
		*retpage = 0;

		if (rwedata->part_set)
			*retcount = rwedata->part.block_count << maux->pages_per_block_shift;
		else
			*retcount = (uint32_t)(ftl_size >> maux->page_shift);

		return 0;
	}

	addr = strtoull(argv[0], &end, 0);
	if (end == argv[0] || *end || addr == ULONG_MAX) {
		os_fprintf(stderr, "Start address is invalid\n");
		return -1;
	}

	if (by_block) {
		if (addr & maux->block_mask) {
			os_fprintf(stderr, "Address must be aligned to block boundary\n");
			return -1;
		}
	} else {
		if (addr & maux->page_mask) {
			os_fprintf(stderr, "Address must be aligned to page boundary\n");
			return -1;
		}
	}

	if (rwedata->part_set && addr >= ((uint64_t)rwedata->part.block_count << maux->block_shift)) {
		os_fprintf(stderr, "Start address exceeds part size\n");
		return -1;
	} else if (!rwedata->part_set && addr >= ftl_size) {
		os_fprintf(stderr, "Start address exceeds flash size\n");
		return -1;
	}

	*retpage = (uint32_t)(addr >> maux->page_shift);

	if (argc == 1) {
		if (for_dump) {
			*retcount = 1;
		} else {
			if (rwedata->part_set)
				size = ((uint64_t)rwedata->part.block_count << maux->pages_per_block_shift) - addr;
			else
				size = ftl_size - addr;

			*retcount = (uint32_t)(size >> maux->page_shift);
		}

		return 1;
	}

	if (!strncmp(argv[1], "count=", 6)) {
		by_count = true;
		sizestr = argv[1] + 6;
	} else {
		sizestr = argv[1];
	}

	size = strtoull(sizestr, &end, 0);
	if (end == sizestr || *end || size == ULONG_MAX) {
		os_fprintf(stderr, "%s is invalid\n", by_count ? "Count" : "Size");
		return -1;
	}

	if (by_count) {
		if (by_block)
			size <<= maux->block_shift;
		else
			size <<= maux->page_shift;
	}

	if (by_block) {
		if (size & maux->block_mask) {
			os_fprintf(stderr, "Size must be multiple of block size\n");
			return -1;
		}
	} else {
		if (size & maux->page_mask) {
			os_fprintf(stderr, "Size must be multiple of page size\n");
			return -1;
		}
	}

	if (rwedata->part_set && (addr + size) > ((uint64_t)rwedata->part.block_count << maux->block_shift)) {
		os_fprintf(stderr, "%s exceeds part size\n", by_count ? "Count" : "Size");
		return -1;
	} else if (!rwedata->part_set && (addr + size) > ftl_size) {
		os_fprintf(stderr, "%s exceeds flash size\n", by_count ? "Count" : "Size");
		return -1;
	}

	*retcount = (uint32_t)(size >> maux->page_shift);

	return 2;
}

static int do_snand_read(void *priv, int argc, char *argv[])
{
	struct ufsnand_instance *inst = priv;
	struct ufnand_rwe_data rwedata;
	uint32_t page, count;
	ufprog_status ret;
	int rc, argp;
	char *file;

	rc = parse_rwe_options(&rwedata, &inst->nand.info.maux, inst->nand.ftl_size, argc, argv);
	if (rc < 0)
		return 1;

	if (argc == rc) {
		os_fprintf(stderr, "File not specified for reading data\n");
		return 1;
	}

	file = argv[rc];
	argp = rc + 1;

	rc = parse_addr_size(&rwedata, &page, &count, &inst->nand.info.maux, false, false, inst->nand.ftl_size,
			     argc - argp, argv + argp);
	if (rc < 0)
		return 1;

	ret = do_nand_read(&inst->nand, &rwedata, page, count, file);
	if (ret)
		return 1;

	return 0;
}

static int do_snand_dump(void *priv, int argc, char *argv[])
{
	struct ufsnand_instance *inst = priv;
	struct ufnand_rwe_data rwedata;
	uint32_t page, count;
	ufprog_status ret;
	int rc;

	if (argc == 1) {
		os_fprintf(stderr, "Dump start address not specified\n");
		return 1;
	}

	if (!strcmp(argv[1], "pp")) {
		hexdump(inst->sinfo.onfi_data, ONFI_PARAM_PAGE_SIZE, 0, false);
		return 0;
	}

	rc = parse_rwe_options(&rwedata, &inst->nand.info.maux, inst->nand.ftl_size, argc, argv);
	if (rc < 0)
		return 1;

	if (argc == rc) {
		os_fprintf(stderr, "Dump address not specified\n");
		return 1;
	}

	rc = parse_addr_size(&rwedata, &page, &count, &inst->nand.info.maux, false, true, inst->nand.ftl_size,
			     argc - rc, argv + rc);
	if (rc < 0)
		return 1;

	ret = do_nand_dump(&inst->nand, &rwedata, page, count);
	if (ret)
		return 1;

	return 0;
}

static int do_snand_write(void *priv, int argc, char *argv[])
{
	struct ufsnand_instance *inst = priv;
	struct ufnand_rwe_data rwedata;
	uint32_t page, count;
	ufprog_status ret;
	int rc, argp;
	char *file;

	rc = parse_rwe_options(&rwedata, &inst->nand.info.maux, inst->nand.ftl_size, argc, argv);
	if (rc < 0)
		return 1;

	if (argc == rc) {
		os_fprintf(stderr, "File not specified for writing data\n");
		return 1;
	}

	file = argv[rc];
	argp = rc + 1;

	rc = parse_addr_size(&rwedata, &page, &count, &inst->nand.info.maux, false, false, inst->nand.ftl_size,
			     argc - argp, argv + argp);
	if (rc < 0)
		return 1;

	ret = do_nand_write(&inst->nand, &rwedata, page, count, file);
	if (ret)
		return 1;

	return 0;
}

static int do_snand_erase(void *priv, int argc, char *argv[])
{
	struct ufsnand_instance *inst = priv;
	struct ufnand_rwe_data rwedata;
	uint32_t page, count;
	ufprog_status ret;
	int rc;

	rc = parse_rwe_options(&rwedata, &inst->nand.info.maux, inst->nand.ftl_size, argc, argv);
	if (rc < 0)
		return 1;

	if (argc == rc) {
		os_fprintf(stderr, "Erase range not specified\n");
		return 1;
	}

	if (!strcmp(argv[rc], "chip")) {
		if (rwedata.part_set) {
			os_printf("Part configuration is suppressed by chip erase.\n");
			rwedata.part_set = false;
		}

		page = 0;
		count = (uint32_t)(inst->nand.ftl_size >> inst->nand.info.maux.page_shift);
	} else {
		rc = parse_addr_size(&rwedata, &page, &count, &inst->nand.info.maux, true, false, inst->nand.ftl_size,
				     argc - rc, argv + rc);
		if (rc < 0)
			return 1;
	}

	ret = do_nand_erase(&inst->nand, &rwedata, page, count);
	if (ret)
		return 1;

	return 0;
}

static int do_snand_markbad(void *priv, int argc, char *argv[])
{
	struct ufsnand_instance *inst = priv;
	ufprog_status ret;
	uint64_t addr;
	char *end;

	if (argc < 2) {
		os_fprintf(stderr, "Flash address not specified\n");
		return 1;
	}

	addr = strtoull(argv[1], &end, 0);
	if (end == argv[1] || *end || addr == ULONG_MAX) {
		os_fprintf(stderr, "Flash address is invalid\n");
		return 1;
	}

	if (addr >= inst->nand.ftl_size) {
		os_fprintf(stderr, "Flash address exceeds flash size\n");
		return 1;
	}

	ret = do_nand_markbad(&inst->nand, addr);
	if (ret)
		return 1;

	return 0;
}

static int do_snand_uid(void *priv, int argc, char *argv[])
{
	struct ufsnand_instance *inst = priv;
	ufprog_status ret;

	ret = do_nand_uid(&inst->nand);
	if (ret)
		return 1;

	return 0;
}

static int do_snand_otp_info(void *priv, int argc, char *argv[])
{
	struct ufsnand_otp_instance *inst = priv;
	ufprog_status ret;

	ret = do_nand_otp_info(inst->nandinst);
	if (ret)
		return 1;

	return 0;
}

static int do_snand_otp_read(void *priv, int argc, char *argv[])
{
	struct ufsnand_otp_instance *inst = priv;
	struct ufnand_rwe_data rwedata;
	ufprog_status ret;
	char *file;
	int rc;

	rc = parse_rwe_options(&rwedata, &inst->nandinst->info.maux, inst->nandinst->ftl_size, argc, argv);
	if (rc < 0)
		return 1;

	if (argc == rc) {
		os_fprintf(stderr, "File not specified for reading OTP page data\n");
		return 1;
	}

	file = argv[rc];

	ret = do_nand_otp_read(inst->nandinst, &rwedata, inst->index, file);
	if (ret)
		return 1;

	return 0;
}

static int do_snand_otp_write(void *priv, int argc, char *argv[])
{
	struct ufsnand_otp_instance *inst = priv;
	struct ufnand_rwe_data rwedata;
	ufprog_status ret;
	char *file;
	int rc;

	rc = parse_rwe_options(&rwedata, &inst->nandinst->info.maux, inst->nandinst->ftl_size, argc, argv);
	if (rc < 0)
		return 1;

	if (argc == rc) {
		os_fprintf(stderr, "File not specified for writing OTP page data\n");
		return 1;
	}

	file = argv[rc];

	ret = do_nand_otp_write(inst->nandinst, &rwedata, inst->index, file);
	if (ret)
		return 1;

	return 0;
}

static int do_snand_otp_lock(void *priv, int argc, char *argv[])
{
	struct ufsnand_otp_instance *inst = priv;
	ufprog_status ret;

	ret = do_nand_otp_lock(inst->nandinst);
	if (ret)
		return 1;

	return 0;
}

static const struct subcmd_entry otp_cmds[] = {
	SUBCMD("info", do_snand_otp_info),
	SUBCMD("read", do_snand_otp_read),
	SUBCMD("write", do_snand_otp_write),
	SUBCMD("lock", do_snand_otp_lock),
};

static int do_snand_otp(void *priv, int argc, char *argv[])
{
	struct ufsnand_instance *inst = priv;
	struct ufsnand_otp_instance otpinst;
	ufprog_bool index_set;
	int exitcode, argp;
	ufprog_status ret;
	uint32_t index;

	if (!inst->nand.info.otp_pages) {
		os_fprintf(stderr, "Not OTP page for this NAND flash chip\n");
		return 1;
	}

	struct cmdarg_entry args[] = {
		CMDARG_U32_OPT_SET("index", index, index_set),
	};

	if (argc == 1) {
		os_fprintf(stderr, "Missing sub-subcommand for otp subcommand\n");
		return 1;
	}

	if (!parse_args(args, ARRAY_SIZE(args), argc, argv, &argp))
		return 1;

	if (index_set) {
		if (index >= inst->nand.info.otp_pages) {
			os_fprintf(stderr, "OTP region index %u is invalid\n", index);
			return false;
		}
	} else {
		if (inst->nand.info.otp_pages > 1 && strcmp(argv[argp], "info")) {
			os_fprintf(stderr, "OTP region index must be specified\n");
			return false;
		}

		index = 0;
	}

	otpinst.index = index;
	otpinst.nandinst = &inst->nand;

	ret = dispatch_subcmd(otp_cmds, ARRAY_SIZE(otp_cmds), &otpinst, argc - argp, argv + argp, &exitcode);
	if (ret) {
		os_fprintf(stderr, "'%s' is not supported by otp subcommand\n", argv[1]);
		return 1;
	}

	return exitcode;
}

static int do_snand_nor_read(void *priv, int argc, char *argv[])
{
	struct ufsnand_instance *inst = priv;
	ufprog_bool enabled = false;
	bool confirmed = false;
	char *confirm_str;
	ufprog_status ret;

	if (!ufprog_spi_nand_supports_nor_read(inst->snand)) {
		os_printf("This SPI-NAND flash chip does not support NOR read timing emulation.\n");
		return 0;
	}

	if (argc == 1) {
		os_fprintf(stderr, "Missing sub-subcommand for nor_read subcommand\n");
		return 1;
	}

	ret = ufprog_spi_nand_nor_read_enabled(inst->snand, &enabled);
	if (ret) {
		os_fprintf(stderr, "Failed to get NOR read timing emulation status.\n");
		return 1;
	}

	if (!strcmp(argv[1], "status")) {
		os_printf("NOR read timing emulation is %s.\n", enabled ? "enabled" : "not enabled yet");
		return 0;
	}

	if (strcmp(argv[1], "enable")) {
		os_fprintf(stderr, "'%s' is not supported by nor_read subcommand\n", argv[1]);
		return 1;
	}

	if (enabled) {
		os_printf("NOR read timing emulation has already been enabled.\n");
		return 0;
	}

	os_printf("Enabling NOR read timing emulation is irreversible.\n");
	os_printf("Are you sure you want to enable it? Type \"confirm\" with enter to continue.\n");

	confirm_str = os_getline_alloc(stdin);
	if (!confirm_str) {
		os_fprintf(stderr, "Failed to read from stdin\n");
		return UFP_FAIL;
	}

	if (!strncmp(confirm_str, "confirm", 7))
		confirmed = true;

	free(confirm_str);

	if (!confirmed) {
		os_fprintf(stderr, "Error: NOR read timing emulation enabling cancelled\n");
		return UFP_OK;
	}

	ret = ufprog_spi_nand_enable_nor_read(inst->snand);
	if (ret) {
		os_fprintf(stderr, "Failed to enable NOR read timing emulation\n");
		return ret;
	}

	os_printf("NOR read timing emulation is permanently enabled now\n");

	return UFP_OK;
}

static const struct subcmd_entry cmds[] = {
	SUBCMD("list", do_snand_list),
	SUBCMD("probe", do_snand_probe),
	SUBCMD("bad", do_snand_bad),
	SUBCMD("read", do_snand_read),
	SUBCMD("dump", do_snand_dump),
	SUBCMD("write", do_snand_write),
	SUBCMD("erase", do_snand_erase),
	SUBCMD("markbad", do_snand_markbad),
	SUBCMD("uid", do_snand_uid),
	SUBCMD("otp", do_snand_otp),
	SUBCMD("nor_read", do_snand_nor_read),
};

static int ufprog_main(int argc, char *argv[])
{
	char *device_name = NULL, *part = NULL, *ftl_cfg = NULL, *bbt_cfg = NULL, *ecc_cfg = NULL;
	struct ufsnand_options nopt;
	const char *last_devname;
	bool list_only = false;
	ufprog_bool die_set;
	int exitcode, argp;
	ufprog_status ret;
	uint32_t die = 0;
	char *devname;

	struct cmdarg_entry args[] = {
		CMDARG_STRING_OPT("dev", device_name),
		CMDARG_STRING_OPT("part", part),
		CMDARG_U32_OPT_SET("die", die, die_set),
		CMDARG_STRING_OPT("ftl", ftl_cfg),
		CMDARG_STRING_OPT("bbt", bbt_cfg),
		CMDARG_STRING_OPT("ecc", ecc_cfg),
	};

	set_os_default_log_print();
	os_init();

	os_printf("Universal flash programmer for SPI-NAND %s %s\n", UFP_VERSION,
		  uses_portable_dirs() ? "[Portable]" : "");
	os_printf("Author: Weijie Gao <hackpascal@gmail.com>\n");
	os_printf("\n");

	ret = load_config(&configs, NULL);
	if (ret)
		return 1;

	set_log_print_level(configs.log_level);

	if (!parse_args(args, ARRAY_SIZE(args), argc, argv, &argp)) {
		show_usage();
		return 1;
	}

	if (argp >= argc) {
		/* No subcommand */
		show_usage();
		return 0;
	}

	ufprog_spi_nand_load_ext_id_file();

	ret = load_config(&configs, device_name);
	if (ret)
		return 1;

	if (!device_name)
		devname = configs.last_device;
	else
		devname = device_name;

	list_only = !strcmp(argv[argp], "list");

	ret = open_device(devname, part, configs.max_speed, &snand_inst, list_only);
	if (ret)
		return 1;

	if (snand_inst.snand && devname) {
		if (configs.last_device)
			last_devname = configs.last_device;
		else
			last_devname = "";

		if (strcmp(devname, last_devname)) {
			memcpy(&nopt, &configs, sizeof(nopt));

			if (snand_inst.max_speed < configs.global_max_speed)
				nopt.max_speed = snand_inst.max_speed;
			else
				nopt.max_speed = configs.global_max_speed;

			nopt.last_device = devname;

			ret = save_config(&nopt);
			if (ret) {
				exitcode = 1;
				goto cleanup;
			}
		}
	}

	if (ufprog_spi_nand_valid(snand_inst.snand)) {
		if (die_set) {
			if (die >= snand_inst.nand.info.memorg.luns_per_cs) {
				if (snand_inst.nand.info.memorg.luns_per_cs > 1) {
					os_fprintf(stderr, "Die ID# %u is invalid. Only %u available\n", die,
						   snand_inst.nand.info.memorg.luns_per_cs);
				} else {
					os_fprintf(stderr, "Die ID# %u is invalid. Only one available\n", die);
				}
			}

			ret = ufprog_nand_select_die(snand_inst.nand.chip, 0, die);
			if (ret) {
				os_fprintf(stderr, "Failed to select Die %u\n", die);
				exitcode = 1;
				goto cleanup;
			}

			if (die)
				os_printf("Selected Die %u\n", die);
		}

		if (ecc_cfg) {
			ret = open_ecc_chip(ecc_cfg, snand_inst.nand.info.memorg.page_size,
					    snand_inst.nand.info.memorg.oob_size, &snand_inst.ecc);
			if (ret) {
				exitcode = 1;
				goto cleanup;
			}

			ret = ufprog_nand_set_ecc(snand_inst.nand.chip, snand_inst.ecc);
			if (ret) {
				exitcode = 1;
				goto cleanup;
			}
		}

		ret = open_bbt(bbt_cfg, snand_inst.nand.chip, &snand_inst.nand.bbt);
		if (ret) {
			exitcode = 1;
			goto cleanup;
		}

		ret = open_ftl(ftl_cfg, snand_inst.nand.chip, snand_inst.nand.bbt, &snand_inst.nand.ftl,
			       &snand_inst.nand.bbt_used);
		if (ret) {
			exitcode = 1;
			goto cleanup;
		}

		snand_inst.nand.ftl_size = ufprog_ftl_get_size(snand_inst.nand.ftl);
	}

	ret = dispatch_subcmd(cmds, ARRAY_SIZE(cmds), &snand_inst, argc - argp, argv + argp, &exitcode);
	if (ret) {
		os_fprintf(stderr, "'%s' is not a supported subcommand\n", argv[1]);
		os_fprintf(stderr, "\n");
		show_usage();
		exitcode = 1;
	}

cleanup:
	ufprog_bbt_modify_config(snand_inst.nand.bbt, BBT_F_READ_ONLY, 0);
	if (snand_inst.nand.bbt_used)
		ufprog_bbt_commit(snand_inst.nand.bbt);

	ufprog_spi_nand_detach(snand_inst.snand, true);
	ufprog_spi_nand_destroy(snand_inst.snand);

	if (snand_inst.ecc)
		ufprog_ecc_free_chip(snand_inst.ecc);

	if (snand_inst.nand.ftl)
		ufprog_ftl_free(snand_inst.nand.ftl);

	if (snand_inst.nand.bbt)
		ufprog_bbt_free(snand_inst.nand.bbt);

	return exitcode;
}

#ifdef _WIN32
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	return os_main(ufprog_main, argc, argv);
}
