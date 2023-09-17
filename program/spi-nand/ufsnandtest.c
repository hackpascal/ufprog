// SPDX-License-Identifier: GPL-2.0-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash test utility
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ufprog/dirs.h>
#include <ufprog/misc.h>
#include <ufprog/sizes.h>
#include <ufprog/osdef.h>
#include "ufsnand-common.h"

struct nand_test_data {
	const struct nand_page_layout *layout;
	uint8_t *buf[2];
	uint8_t *map;
	uint32_t seed;
	uint32_t start_block;
	uint32_t block_count;
	bool raw;
	bool oob;
	bool fmt;
};

static struct ufsnand_options configs;
static struct ufsnand_instance snand_inst;

static const char usage[] =
	"Usage:\n"
	"    %s [dev=<dev>] [part=<partmodel>] [ecc=<ecccfg>] [raw] [oob] [fmt]\n"
	"       [addr=<addr>] [len=<len>] all\n"
	"\n"
	"Global options:\n"
	"        dev  - Specify the device to be opened.\n"
	"               If not specified, last device recorded in config will be used.\n"
	"        ecc  - Specify the ECC engine for page read/write.\n"
	"               Its value can be one of the following type:\n"
	"                 none: Do not use ECC engine\n"
	"                 <ecc-plugin>: Use specified ECC engine plugin\n"
	"                 <ecc-plugin>,<config>: Use specified ECC engine plugin with\n"
	"                                        configuration file\n"
	"               If not specified, default ECC engine provided by the spi-nand\n"
	"               controller will be used. The default ECC engine may be the\n"
	"               On-die ECC engine if supported, or 'none'.\n"
	"        raw  - Do not enable ECC for read/write.\n"
	"        oob  - Test also the OOB region.\n"
	"        fmt  - Use canonical page layout from ECC engine if possible.\n"
	"        addr - Specify the start address for test. Default is 0.\n"
	"               This address will be rounded down to block boundary.\n"
	"        len  - Specify the size for test starting from <addr>. Default is\n"
	"               the whole flash size.\n"
	"               This address will be rounded up to block boundary.\n"
	"\n"
	"Test items:\n"
	"    all - Confirm to test\n"
	"\n"
	"Only main data array read/write/erase is available for NAND flash.\n"
	"\n"
	"WARNING: ALL DATA ON FLASH WILL BE LOST!\n";

static void show_usage(void)
{
	os_printf(usage, os_prog_name());
}

static void gen_pat(uint8_t *pat, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		pat[i] = (uint8_t)(rand() * UCHAR_MAX / RAND_MAX);
}

static ufprog_status nand_test_erase_flash(struct ufnand_instance *nandinst, struct nand_test_data *ntd)
{
	uint32_t block = ntd->start_block, end = block + ntd->block_count, bcnt = 0, percentage, last_percentage = 0;
	ufprog_status ret = UFP_OK;
	uint64_t t0, t1;

	progress_init();

	t0 = os_get_timer_us();

	while (block < end) {
		if (ufprog_bbt_is_bad(nandinst->bbt, block))
			goto next_block;

		ret = ufprog_nand_erase_block(nandinst->chip, block << nandinst->info.maux.pages_per_block_shift);
		if (ret) {
			os_fprintf(stderr, "Failed to erase block %u at %" PRIx64 "\n", block,
				   (uint64_t)block << nandinst->info.maux.block_shift);
			break;
		}

		ufprog_bbt_set_state(nandinst->bbt, block, BBT_ST_ERASED);

	next_block:
		block++;
		bcnt++;

		percentage = (uint32_t)((bcnt * 100ULL) / ntd->block_count);
		if (percentage > last_percentage) {
			last_percentage = percentage;
			progress_show(last_percentage);
		}
	}

	if (!ret) {
		t1 = os_get_timer_us();

		progress_done();
		print_speed((uint64_t)ntd->block_count << nandinst->info.maux.block_shift, t1 - t0);
		os_printf("Succeeded\n");
	}

	return ret;
}

static ufprog_status nand_test_check_page_bitflips(struct ufnand_instance *nandinst, uint8_t *buf, uint32_t page,
						   bool oob)
{
	uint32_t i, page_size, cnt = 0;
	uint8_t n;

	page_size = nandinst->info.memorg.page_size;
	if (oob)
		page_size += nandinst->info.memorg.oob_size;

	for (i = 0; i < page_size; i++) {
		if (buf[i] != 0xff) {
			n = 0xff ^ buf[i];
			cnt += hweight8(n);
		}
	}

	if (cnt == 1)
		os_printf("1 bitflip found in page %u\n", page);
	else if (cnt > 1)
		os_printf("%u bitflips found in page %u\n", cnt, page);

	return UFP_OK;
}

static ufprog_status nand_test_check_block_bitflips(struct ufnand_instance *nandinst, uint8_t *buf, uint32_t page,
						    bool oob)
{
	uint32_t i, rdcnt;
	ufprog_status ret;

	ret = ufprog_nand_read_pages(nandinst->chip, page, nandinst->info.memorg.pages_per_block, buf, true, 0, &rdcnt);
	if (ret) {
		os_fprintf(stderr, "Failed to read page %u at %" PRIx64 "\n", page + rdcnt,
			   (uint64_t)(page + rdcnt) << nandinst->info.maux.page_shift);
		return ret;
	}

	for (i = 0; i < nandinst->info.memorg.pages_per_block; i++) {
		ret = nand_test_check_page_bitflips(nandinst, buf + i * nandinst->info.maux.oob_page_size,
						    page + i, oob);
		if (ret) {
			os_fprintf(stderr, "Failed to check page %u at %" PRIx64 "\n", page + i,
				   (uint64_t)(page + i) << nandinst->info.maux.page_shift);
			return ret;
		}
	}

	return UFP_OK;
}

static ufprog_status nand_test_check_bitflips(struct ufnand_instance *nandinst, struct nand_test_data *ntd)
{
	uint32_t block = ntd->start_block, end = block + ntd->block_count, bcnt = 0, percentage, last_percentage = 0;
	ufprog_status ret = UFP_OK;
	uint64_t t0, t1;

	progress_init();

	t0 = os_get_timer_us();

	while (block < end) {
		if (ufprog_bbt_is_bad(nandinst->bbt, block))
			goto next_block;

		ret = nand_test_check_block_bitflips(nandinst, ntd->buf[0],
						     block << nandinst->info.maux.pages_per_block_shift, ntd->oob);
		if (ret) {
			os_fprintf(stderr, "Failed to check block %u at %" PRIx64 "\n", block,
				   (uint64_t)block << nandinst->info.maux.block_shift);
			break;
		}

	next_block:
		block++;
		bcnt++;

		percentage = (uint32_t)((bcnt * 100ULL) / ntd->block_count);
		if (percentage > last_percentage) {
			last_percentage = percentage;
			progress_show(last_percentage);
		}
	}

	if (!ret) {
		t1 = os_get_timer_us();

		progress_done();
		print_speed((uint64_t)ntd->block_count << nandinst->info.maux.block_shift, t1 - t0);
		os_printf("Done\n");
	}

	return ret;
}

static ufprog_status nand_test_generate_block_pattern(struct ufnand_instance *nandinst, struct nand_test_data *ntd,
						      uint8_t *dst, uint8_t *tmp, uint8_t pat_xor)
{
	uint8_t *p = dst, *pat = tmp, *fmtpat = pat + nandinst->info.maux.oob_page_size;
	uint32_t i, j, flags = PAGE_FILL_F_FILL_NON_DATA_FF;
	ufprog_status ret;

	for (i = 0; i < nandinst->info.memorg.pages_per_block; i++) {
		gen_pat(pat, nandinst->info.maux.oob_page_size);

		for (j = 0; j < nandinst->info.maux.oob_page_size; j++)
			pat[j] ^= pat_xor;

		if (ntd->oob) {
			flags |= PAGE_FILL_F_FILL_OOB;

			if (ntd->raw) {
				flags |= PAGE_FILL_F_FILL_UNPROTECTED_OOB | PAGE_FILL_F_FILL_UNUSED |
					PAGE_FILL_F_FILL_ECC_PARITY;
			}
		}

		if (!ntd->fmt) {
			ufprog_nand_fill_page_by_layout(ntd->layout, p, pat, nandinst->info.maux.oob_page_size, flags);
		} else {
			ufprog_nand_fill_page_by_layout(ntd->layout, fmtpat, pat, nandinst->info.maux.oob_page_size,
							flags);

			ret = ufprog_nand_convert_page_format(nandinst->chip, fmtpat, p, true);
			if (ret) {
				os_fprintf(stderr, "Failed to convert page data\n");
				return ret;
			}
		}

		p += nandinst->info.maux.oob_page_size;
	}

	return UFP_OK;
}

static ufprog_status nand_test_write_block_pattern(struct ufnand_instance *nandinst, struct nand_test_data *ntd,
						   uint32_t page, uint8_t pat_xor)
{
	ufprog_status ret;
	uint32_t wrcnt;

	ret = nand_test_generate_block_pattern(nandinst, ntd, ntd->buf[0], ntd->buf[1], pat_xor);
	if (ret)
		return ret;

	ret = ufprog_nand_write_pages(nandinst->chip, page, nandinst->info.memorg.pages_per_block, ntd->buf[0],
				      ntd->raw, false, &wrcnt);
	if (ret) {
		os_fprintf(stderr, "Failed to write page %u at %" PRIx64 "\n", page + wrcnt,
			   (uint64_t)(page + wrcnt) << nandinst->info.maux.page_shift);
		return ret;
	}

	return UFP_OK;
}

static ufprog_status nand_test_write_pattern(struct ufnand_instance *nandinst, struct nand_test_data *ntd,
					     uint8_t pat_xor)
{
	uint32_t block = ntd->start_block, end = block + ntd->block_count, bcnt = 0, percentage, last_percentage = 0;
	ufprog_status ret = UFP_OK;
	uint64_t t0, t1;

	srand(ntd->seed);

	progress_init();

	t0 = os_get_timer_us();

	while (block < end) {
		if (ufprog_bbt_is_bad(nandinst->bbt, block))
			goto next_block;

		ret = nand_test_write_block_pattern(nandinst, ntd, block << nandinst->info.maux.pages_per_block_shift,
						    pat_xor);
		if (ret) {
			os_fprintf(stderr, "Failed to write block %u at %" PRIx64 "\n", block,
				   (uint64_t)block << nandinst->info.maux.block_shift);
			break;
		}

	next_block:
		block++;
		bcnt++;

		percentage = (uint32_t)((bcnt * 100ULL) / ntd->block_count);
		if (percentage > last_percentage) {
			last_percentage = percentage;
			progress_show(last_percentage);
		}
	}

	if (!ret) {
		t1 = os_get_timer_us();

		progress_done();
		print_speed((uint64_t)ntd->block_count << nandinst->info.maux.block_shift, t1 - t0);
		os_printf("Succeeded\n");
	}

	return ret;
}

static ufprog_status nand_test_compare_page(struct ufnand_instance *nandinst, struct nand_test_data *ntd,
					    uint8_t *buf, const uint8_t *pat, uint32_t page)
{
	uint32_t i, cnt = 0;
	bool check;
	uint8_t n;

	for (i = 0; i < nandinst->info.maux.oob_page_size; i++) {
		check = false;

		if (ntd->map[i] == NAND_PAGE_BYTE_DATA) {
			check = true;
		} else if (ntd->oob) {
			if (ntd->raw) {
				if (ntd->map[i] != NAND_PAGE_BYTE_MARKER)
					check = true;
			} else {
				if (ntd->map[i] == NAND_PAGE_BYTE_OOB_DATA)
					check = true;
			}
		}

		if (check && buf[i] != pat[i]) {
			n = pat[i] ^ buf[i];
			cnt += hweight8(n);
		}
	}

	if (ntd->raw) {
		if (cnt == 1)
			os_printf("1 bitflip found in page %u\n", page);
		else if (cnt > 1)
			os_printf("%u bitflips found in page %u\n", cnt, page);
	} else {
		if (cnt == 1)
			os_fprintf(stderr, "Error: 1 bitflip found in page %u after ECC decoding\n", page);
		else if (cnt > 1)
			os_fprintf(stderr, "Error: %u bitflips found in page %u after ECC decoding\n", cnt, page);
	}

	return UFP_OK;
}

static ufprog_status nand_test_verify_block_pattern(struct ufnand_instance *nandinst, struct nand_test_data *ntd,
						    uint32_t page, uint8_t pat_xor)
{
	uint32_t i, rdcnt;
	ufprog_status ret;

	ret = nand_test_generate_block_pattern(nandinst, ntd, ntd->buf[0], ntd->buf[1], pat_xor);
	if (ret)
		return ret;

	ret = ufprog_nand_read_pages(nandinst->chip, page, nandinst->info.memorg.pages_per_block, ntd->buf[1], ntd->raw,
				     NAND_READ_F_IGNORE_ECC_ERROR, &rdcnt);
	if (ret) {
		os_fprintf(stderr, "Failed to read page %u at %" PRIx64 "\n", page + rdcnt,
			   (uint64_t)(page + rdcnt) << nandinst->info.maux.page_shift);
		return ret;
	}

	for (i = 0; i < nandinst->info.memorg.pages_per_block; i++) {
		ret = nand_test_compare_page(nandinst, ntd, ntd->buf[1] + i * nandinst->info.maux.oob_page_size,
					     ntd->buf[0] + i * nandinst->info.maux.oob_page_size, page + i);
		if (ret) {
			os_fprintf(stderr, "Failed to verify page %u at %" PRIx64 "\n", page + i,
				   (uint64_t)(page + i) << nandinst->info.maux.page_shift);
			return ret;
		}
	}

	return UFP_OK;
}

static void nand_test_check_zero_page(struct ufnand_instance *nandinst, struct nand_test_data *ntd, const uint8_t *buf,
				      uint32_t page)
{
	uint32_t i, cnt = 0;
	bool check;

	for (i = 0; i < nandinst->info.maux.oob_page_size; i++) {
		check = false;

		if (ntd->map[i] == NAND_PAGE_BYTE_DATA) {
			check = true;
		} else if (ntd->oob) {
			if (ntd->raw) {
				if (ntd->map[i] != NAND_PAGE_BYTE_MARKER)
					check = true;
			} else {
				if (ntd->map[i] == NAND_PAGE_BYTE_OOB_DATA)
					check = true;
			}
		}

		if (check && buf[i])
			cnt += hweight8(buf[i]);
	}

	if (cnt == 1)
		os_printf("1 bitflip found in page %u\n", page);
	else if (cnt > 1)
		os_printf("%u bitflips found in page %u\n", cnt, page);
}

static ufprog_status nand_test_check_zero_block(struct ufnand_instance *nandinst, struct nand_test_data *ntd,
						uint32_t page)
{
	uint32_t i, rdcnt;
	ufprog_status ret;

	ret = ufprog_nand_read_pages(nandinst->chip, page, nandinst->info.memorg.pages_per_block, ntd->buf[0], ntd->raw,
				     NAND_READ_F_IGNORE_ECC_ERROR, &rdcnt);
	if (ret) {
		os_fprintf(stderr, "Failed to read page %u at %" PRIx64 "\n", page + rdcnt,
			   (uint64_t)(page + rdcnt) << nandinst->info.maux.page_shift);
		return ret;
	}

	for (i = 0; i < nandinst->info.memorg.pages_per_block; i++) {
		nand_test_check_zero_page(nandinst, ntd, ntd->buf[0] + i * nandinst->info.maux.oob_page_size,
					  page + i);
	}

	return UFP_OK;
}

static ufprog_status nand_test_verify_pattern(struct ufnand_instance *nandinst, struct nand_test_data *ntd,
					      bool check_zero, uint8_t pat_xor)
{
	uint32_t block = ntd->start_block, end = block + ntd->block_count, bcnt = 0, percentage, last_percentage = 0;
	ufprog_status ret = UFP_OK;
	uint64_t t0, t1;

	srand(ntd->seed);

	progress_init();

	t0 = os_get_timer_us();

	while (block < end) {
		if (ufprog_bbt_is_bad(nandinst->bbt, block))
			goto next_block;

		if (check_zero)
			ret = nand_test_check_zero_block(nandinst, ntd,
							 block << nandinst->info.maux.pages_per_block_shift);
		else
			ret = nand_test_verify_block_pattern(nandinst, ntd,
							     block << nandinst->info.maux.pages_per_block_shift,
							     pat_xor);

		if (ret) {
			os_fprintf(stderr, "Failed to verify block %u at %" PRIx64 "\n", block,
				   (uint64_t)block << nandinst->info.maux.block_shift);
			break;
		}

	next_block:
		block++;
		bcnt++;

		percentage = (uint32_t)((bcnt * 100ULL) / ntd->block_count);
		if (percentage > last_percentage) {
			last_percentage = percentage;
			progress_show(last_percentage);
		}
	}

	if (!ret) {
		t1 = os_get_timer_us();

		progress_done();
		print_speed((uint64_t)ntd->block_count << nandinst->info.maux.block_shift, t1 - t0);
		os_printf("Succeeded\n");
	}

	return ret;
}

static int nand_test_rw(struct ufnand_instance *nandinst, uint64_t addr, uint64_t len, bool raw, bool oob, bool fmt)
{
	struct ufprog_nand_ecc_chip *ecc;
	struct nand_test_data ntd;
	bool dfl_layout = false;
	ufprog_status ret;
	int exitcode = 1;
	uint64_t end;

	memset(&ntd, 0, sizeof(ntd));

	if (addr >= nandinst->info.maux.size) {
		os_fprintf(stderr, "Invalid test address\n");
		return 1;
	}

	if (!len)
		len = nandinst->info.maux.size - addr;

	end = ((addr + len) & ~nandinst->info.maux.block_mask);
	addr &= ~nandinst->info.maux.block_mask;

	ntd.start_block = (uint32_t)(addr >> nandinst->info.maux.block_shift);
	ntd.block_count = (uint32_t)((end - addr) >> nandinst->info.maux.block_shift);

	if (!ntd.block_count) {
		os_printf("Nothing to test\n");
		return 0;
	}

	ntd.seed = (unsigned int)time(NULL);
	ntd.raw = raw;
	ntd.oob = oob;
	ntd.fmt = fmt;

	ecc = ufprog_nand_get_ecc(nandinst->chip);
	if (ecc) {
		ntd.layout = ufprog_ecc_get_page_layout(ecc, fmt && ufprog_ecc_support_convert_page_layout(ecc));
	} else {
		os_printf("Chip does not support ECC\n\n");
		ntd.raw = true;
	}

	if (!ntd.layout) {
		ret = ufprog_nand_generate_page_layout(nandinst->chip, (struct nand_page_layout **)&ntd.layout);
		if (ret) {
			os_fprintf(stderr, "Failed to generate default page layout\n");
			return 1;
		}

		dfl_layout = true;
	}

	ntd.buf[0] = malloc(nandinst->info.maux.oob_block_size * 2 + nandinst->info.maux.oob_page_size);
	if (!ntd.buf[0]) {
		os_fprintf(stderr, "No memory for R/W test buffer\n");
		goto cleanup_layout;
	}

	ntd.buf[1] = ntd.buf[0] + nandinst->info.maux.oob_block_size;
	ntd.map = ntd.buf[1] + nandinst->info.maux.oob_block_size;

	ufprog_nand_page_layout_to_map(ntd.layout, ntd.map);

	os_printf("[ Flash regular Read/Write/Erase test ]\n");
	os_printf("Range: 0x%" PRIx64 " - 0x%" PRIx64 "\n", addr, end);
	os_printf("\n");

	ret = ufprog_bbt_ram_create("bbt", nandinst->chip, &nandinst->bbt);
	if (ret) {
		os_fprintf(stderr, "Failed to create bad block table\n");
		goto cleanup_buf;
	}

	ret = ufprog_bbt_modify_config(nandinst->bbt, 0, BBT_F_FULL_SCAN);
	if (ret) {
		os_fprintf(stderr, "Failed to configure bad block table\n");
		goto cleanup_bbt;
	}

	os_printf("Scanning bad blocks ...\n");

	ret = ufprog_bbt_reprobe(nandinst->bbt);
	if (ret) {
		os_fprintf(stderr, "Failed to scan bad block\n");
		goto cleanup_bbt;
	}

	print_bbt(nandinst, nandinst->bbt);
	os_printf("\n");

	os_printf("Using seed %u\n", ntd.seed);
	os_printf("\n");

	os_printf("1. Erase whole flash\n");

	ret = nand_test_erase_flash(nandinst, &ntd);
	if (ret)
		goto cleanup_bbt;
	os_printf("\n");

	os_printf("2. Check bitflips after erase\n");

	ret = nand_test_check_bitflips(nandinst, &ntd);
	if (ret)
		goto cleanup_bbt;
	os_printf("\n");

	os_printf("3. Writing random pattern\n");

	ret = nand_test_write_pattern(nandinst, &ntd, 0);
	if (ret)
		goto cleanup_bbt;
	os_printf("\n");

	os_printf("4. Verifing pattern\n");

	ret = nand_test_verify_pattern(nandinst, &ntd, false, 0);
	if (ret)
		goto cleanup_bbt;
	os_printf("\n");

	if (!raw || !nandinst->info.random_page_write || nandinst->info.nops == 1)
		goto end;

	os_printf("5. Writing complementary pattern\n");

	ret = nand_test_write_pattern(nandinst, &ntd, 0xff);
	if (ret)
		goto cleanup_bbt;
	os_printf("\n");

	os_printf("6. Verifing complementary pattern\n");

	ret = nand_test_verify_pattern(nandinst, &ntd, true, 0xff);
	if (ret)
		goto cleanup_bbt;
	os_printf("\n");

end:
	os_printf("R/W test passed\n");
	os_printf("\n");

	exitcode = 0;

cleanup_bbt:
	ufprog_bbt_free(nandinst->bbt);
	nandinst->bbt = NULL;

cleanup_buf:
	free(ntd.buf[0]);

cleanup_layout:
	if (dfl_layout)
		ufprog_nand_free_page_layout((struct nand_page_layout *)ntd.layout);

	return exitcode;
}

static int ufprog_main(int argc, char *argv[])
{
	ufprog_bool test_all = false, raw = false, oob = false, fmt = false;
	char *device_name = NULL, *part = NULL, *ecc_cfg = NULL;
	struct ufsnand_options nopt;
	uint64_t addr = 0, len = 0;
	const char *last_devname;
	int exitcode = 0, argp;
	ufprog_status ret;
	char *devname;

	struct cmdarg_entry args[] = {
		CMDARG_STRING_OPT("dev", device_name),
		CMDARG_STRING_OPT("part", part),
		CMDARG_BOOL_OPT("all", test_all),
		CMDARG_STRING_OPT("ecc", ecc_cfg),
		CMDARG_BOOL_OPT("raw", raw),
		CMDARG_BOOL_OPT("oob", oob),
		CMDARG_BOOL_OPT("fmt", fmt),
		CMDARG_U64_OPT("addr", addr),
		CMDARG_U64_OPT("len", len),
	};

	set_os_default_log_print();
	os_init();

	os_printf("Universal flash programmer for SPI-NAND %s %s\n", UFP_VERSION,
		  uses_portable_dirs() ? "[Portable]" : "");
	os_printf("Flash Test Utility\n");
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

	if (!test_all) {
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

	ret = open_device(devname, part, configs.max_speed, &snand_inst, false);
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
				goto out;
			}
		}
	}

	if (ecc_cfg) {
		ret = open_ecc_chip(ecc_cfg, snand_inst.nand.info.memorg.page_size,
				    snand_inst.nand.info.memorg.oob_size, &snand_inst.ecc);
		if (ret) {
			exitcode = 1;
			goto out;
		}

		ret = ufprog_nand_set_ecc(snand_inst.nand.chip, snand_inst.ecc);
		if (ret) {
			exitcode = 1;
			goto out;
		}
	}

	exitcode = nand_test_rw(&snand_inst.nand, addr, len, raw, oob, fmt);

	os_printf("[ Flash test finished ]\n");

out:
	ufprog_spi_nand_detach(snand_inst.snand, true);
	ufprog_spi_nand_destroy(snand_inst.snand);

	if (snand_inst.ecc)
		ufprog_ecc_free_chip(snand_inst.ecc);

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
