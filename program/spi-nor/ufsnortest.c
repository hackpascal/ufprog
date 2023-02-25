// SPDX-License-Identifier: GPL-2.0-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash test utility
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ufprog/dirs.h>
#include <ufprog/misc.h>
#include <ufprog/sizes.h>
#include <ufprog/osdef.h>
#include "ufsnor-common.h"

static struct ufsnor_options configs;
static struct ufsnor_instance snor_inst;

static const char usage[] =
	"Usage:\n"
	"    %s [dev=<dev>] [part=<partmodel>] <test item> [<test item>...]\n"
	"\n"
	"Test items:\n"
	"    all - Test all following itemsn\n"
	"    rw - Test read/write/erase on main storage.\n"
	"    otp - Test read/write/erase on OTP region(s).\n"
	"          This only allowed if the OTP region supports erase operation.\n"
	"    wp - Test write-protect bits\n"
	"\n"
	"WARNING: ALL DATA ON FLASH WILL BE LOST!\n";

static void show_usage(void)
{
	os_printf(usage, os_prog_name());
}

static void gen_pat(uint8_t *buf, size_t len)
{
	size_t i;

	srand((unsigned int)time(NULL));

	for (i = 0; i < len; i++)
		buf[i] = (uint8_t)(rand() * UCHAR_MAX / RAND_MAX);
}

static int snor_test_rw(struct ufsnor_instance *inst)
{
	uint8_t *buf, *test_pat;
	size_t test_size, i;
	ufprog_status ret;
	int exitcode = 1;
	uint64_t opsize;

	os_printf("[ Flash regular Read/Write/Erase test ]\n");
	os_printf("\n");

	inst->die_start = 0;
	inst->die_count = inst->info.ndies;

	opsize = inst->info.size * (uint64_t)inst->info.ndies;

	if (opsize > SIZE_MAX / 2)
		test_size = SIZE_MAX / 2;
	else
		test_size = opsize;

	buf = malloc(test_size * 2);
	if (!buf) {
		os_fprintf(stderr, "No memory for R/W test buffer\n");
		return 1;
	}

	test_pat = buf + test_size;

	os_printf("1. Erase whole flash\n");

	ret = erase_flash(inst, 0, test_size);
	if (ret)
		goto out;
	os_printf("\n");

	os_printf("2. Verifying if all data bytes are FFh after erase\n");

	ret = read_flash(inst, 0, test_size, buf);
	if (ret)
		goto out;
	os_printf("\n");

	for (i = 0; i < test_size; i++) {
		if (buf[i] != 0xff) {
			os_fprintf(stderr, "Data at 0x%zx is not 0xff\n", i);
			goto out;
		}
	}

	gen_pat(test_pat, test_size);

	os_printf("3. Writing random pattern and verify\n");

	ret = write_flash_no_erase(inst, 0, test_size, test_pat, true);
	if (ret)
		goto out;
	os_printf("\n");

	memset(test_pat, 0, test_size);

	os_printf("4. Writing zero data and verify\n");

	ret = write_flash_no_erase(inst, 0, test_size, test_pat, true);
	if (ret)
		goto out;
	os_printf("\n");

	os_printf("R/W test passed\n");
	os_printf("\n");

	exitcode = 0;

out:
	free(buf);

	return exitcode;
}

static ufprog_status snor_test_otp_region(struct ufsnor_instance *inst, uint32_t index)
{
	uint8_t *buf, *test_pat;
	uint32_t test_size, i;
	ufprog_status ret;
	int exitcode = 1;

	os_printf("Testing OTP region %u:\n", index);

	test_size = inst->info.otp->size;

	buf = malloc(test_size * 2);
	if (!buf) {
		os_fprintf(stderr, "No memory for R/W test buffer\n");
		return 1;
	}

	test_pat = buf + test_size;

	os_printf("    1. Erase whole OTP region\n");

	ret = ufprog_spi_nor_otp_erase(inst->snor, index);
	if (ret) {
		os_fprintf(stderr, "Failed to erase OTP region %u\n", index);
		goto out;
	}
	os_printf("       Succeeded\n");

	os_printf("    2. Verify if all data bytes are FFh after erase\n");

	ret = ufprog_spi_nor_otp_read(inst->snor, index, 0, test_size, buf);
	if (ret) {
		os_fprintf(stderr, "Failed to read OTP region %u\n", index);
		goto out;
	}

	for (i = 0; i < test_size; i++) {
		if (buf[i] != 0xff) {
			os_fprintf(stderr, "Data at 0x%x is not 0xff\n", i);
			goto out;
		}
	}
	os_printf("       Succeeded\n");

	gen_pat(test_pat, test_size);

	os_printf("    3. Write random pattern\n");

	ret = ufprog_spi_nor_otp_write(inst->snor, index, 0, test_size, test_pat);
	if (ret) {
		os_fprintf(stderr, "Failed to write OTP region %u\n", index);
		goto out;
	}
	os_printf("       Succeeded\n");

	os_printf("    4. Verify pattern\n");

	ret = ufprog_spi_nor_otp_read(inst->snor, index, 0, test_size, buf);
	if (ret) {
		os_fprintf(stderr, "Failed to read OTP region %u\n", index);
		goto out;
	}

	for (i = 0; i < test_size; i++) {
		if (test_pat[i] != buf[i]) {
			log_err("Data at 0x%x is different: expected 0x%02x, got 0x%02x\n", i, test_pat[i], buf[i]);
			goto out;
		}
	}
	os_printf("       Succeeded\n");

	ufprog_spi_nor_otp_erase(inst->snor, index);

	os_printf("OTP region %u test passed\n", index);

	exitcode = 0;

out:
	free(buf);

	return exitcode;
}

static int snor_test_otp_die(struct ufsnor_instance *inst, uint32_t die)
{
	ufprog_bool locked;
	ufprog_status ret;
	int exitcode = 0;
	uint32_t i;

	ret = ufprog_spi_nor_select_die(inst->snor, die);
	if (ret) {
		os_fprintf(stderr, "Failed to select Die %u\n", die);
		return 1;
	}

	if (inst->info.ndies > 1)
		os_printf("Selected Die %u:\n\n", die);

	for (i = 0; i < inst->info.otp->count; i++) {
		ret = ufprog_spi_nor_otp_locked(inst->snor, inst->info.otp->start_index + i, &locked);
		if (ret) {
			os_fprintf(stderr, "Failed to get lock status of OTP region %u\n",
				   inst->info.otp->start_index + i);
			continue;
		}

		if (locked) {
			os_printf("OTP region %u is locked. Test skipped.\n", inst->info.otp->start_index + i);
			continue;
		}

		ret = snor_test_otp_region(inst, inst->info.otp->start_index + i);
		if (ret)
			exitcode = 1;

		os_printf("\n");
	}

	return exitcode;
}

static int snor_test_otp(struct ufsnor_instance *inst)
{
	ufprog_status ret;
	uint32_t die;

	if (!inst->info.otp) {
		os_fprintf(stderr, "[ Flash OTP not supported ]\n");
		return 0;
	}

	if (!inst->info.otp_erasable) {
		os_fprintf(stderr, "[ Flash OTP Read/Write/Erase test skipped ]\n");
		return 0;
	}

	os_printf("[ Flash OTP Read/Write/Erase test ]\n");
	os_printf("\n");

	for (die = 0; die < inst->info.ndies; die++) {
		ret = snor_test_otp_die(inst, die);
		if (ret) {
			os_printf("OTP region test failed on Die %u\n", die);
			return 1;
		}
	}

	return 0;
}

static int snor_test_wp_die(struct ufsnor_instance *inst, struct spi_nor_wp_regions *regions, uint32_t die)
{
	struct spi_nor_wp_region rg;
	uint32_t i, width = 6;
	ufprog_status ret;
	int exitcode = 0;

	ret = ufprog_spi_nor_select_die(inst->snor, die);
	if (ret) {
		os_fprintf(stderr, "Failed to select Die %u\n", die);
		return 1;
	}

	if (inst->info.ndies > 1)
		os_printf("Selected Die %u:\n\n", die);

	if (inst->info.size > 0x1000000)
		width = 8;

	for (i = 0; i < regions->num; i++) {
		if (!regions->region[i].size)
			continue;

		if (regions->region[i].size == inst->info.size) {
			os_printf("Testing ALL\n");
		} else {
			os_printf("Testing %0*" PRIX64 "h - %0*" PRIX64 "h\n", width, regions->region[i].base, width,
				  regions->region[i].base + regions->region[i].size - 1);
		}

		ret = ufprog_spi_nor_set_wp_region(inst->snor, &regions->region[i]);
		if (ret) {
			os_fprintf(stderr, "Failed to set write-protect region\n");
			exitcode = 1;
			continue;
		}

		ret = ufprog_spi_nor_get_wp_region(inst->snor, &rg);
		if (ret) {
			os_fprintf(stderr, "Failed to get write-protect region\n");
			exitcode = 1;
			continue;
		}

		if (rg.base == regions->region[i].base && rg.size == regions->region[i].size) {
			os_printf("    Passed\n");
		} else {
			os_printf("    Mismatch\n");
			exitcode = 1;
		}
	}

	rg.base = rg.size = 0;

	ret = ufprog_spi_nor_set_wp_region(inst->snor, &rg);
	if (ret) {
		os_fprintf(stderr, "Failed to clear write-protect region\n");
		exitcode = 1;
	}

	os_printf("\n");

	return exitcode;
}

static int snor_test_wp(struct ufsnor_instance *inst)
{
	struct spi_nor_wp_regions regions;
	ufprog_status ret;
	uint32_t die;

	ret = ufprog_spi_nor_get_wp_region_list(inst->snor, &regions);
	if (ret) {
		if (ret == UFP_UNSUPPORTED) {
			os_fprintf(stderr, "[ Write-protect not supported ]\n");
			return 0;
		}

		os_fprintf(stderr, "Failed to get write-protect regions\n");
		return 1;
	}

	os_printf("[ Flash Write-protect test ]\n");
	os_printf("\n");

	for (die = 0; die < inst->info.ndies; die++) {
		ret = snor_test_wp_die(inst, &regions, die);
		if (ret) {
			os_printf("Write-protect region test failed on Die %u\n", die);
			return 1;
		}
	}

	return 0;
}

static int ufprog_main(int argc, char *argv[])
{
	ufprog_bool test_all = false, test_rw = false, test_otp = false, test_wp = false;
	char *device_name = NULL, *part = NULL;
	struct ufsnor_options nopt;
	const char *last_devname;
	int exitcode = 0, argp;
	ufprog_status ret;
	char *devname;

	struct cmdarg_entry args[] = {
		CMDARG_STRING_OPT("dev", device_name),
		CMDARG_STRING_OPT("part", part),
		CMDARG_BOOL_OPT("all", test_all),
		CMDARG_BOOL_OPT("rw", test_rw),
		CMDARG_BOOL_OPT("otp", test_otp),
		CMDARG_BOOL_OPT("wp", test_wp),
	};

	set_os_default_log_print();
	os_init();

	os_printf("Universal flash programmer for SPI-NOR %s %s\n", UFP_VERSION,
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

	if (test_all) {
		test_rw = true;
		test_otp = true;
		test_wp = true;
	}

	if (!test_rw && !test_otp && !test_wp) {
		/* No subcommand */
		show_usage();
		return 0;
	}

	ufprog_spi_nor_load_ext_id_file();

	ret = load_config(&configs, device_name);
	if (ret)
		return 1;

	if (!device_name)
		devname = configs.last_device;
	else
		devname = device_name;

	ret = open_device(devname, part, configs.max_speed, &snor_inst, false);
	if (ret)
		return 1;

	if (snor_inst.snor && devname) {
		if (configs.last_device)
			last_devname = configs.last_device;
		else
			last_devname = "";

		if (strcmp(devname, last_devname)) {
			memcpy(&nopt, &configs, sizeof(nopt));

			if (snor_inst.max_speed < configs.global_max_speed)
				nopt.max_speed = snor_inst.max_speed;
			else
				nopt.max_speed = configs.global_max_speed;

			nopt.last_device = devname;

			ret = save_config(&nopt);
			if (ret)
				return 1;
		}
	}

	if (test_rw) {
		exitcode = snor_test_rw(&snor_inst);
		if (exitcode)
			goto out;
	}

	if (test_otp) {
		exitcode = snor_test_otp(&snor_inst);
		if (exitcode)
			goto out;
	}

	if (test_wp) {
		exitcode = snor_test_wp(&snor_inst);
		if (exitcode)
			goto out;
	}

	os_printf("[ Flash test finished ]\n");

out:
	ufprog_spi_nor_detach(snor_inst.snor, true);
	ufprog_spi_nor_destroy(snor_inst.snor);

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
