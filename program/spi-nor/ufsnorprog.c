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
#include "ufsnor-common.h"

#define DEFAULT_UID_MAX_LEN				32

struct ufsnor_otp_instance {
	struct spi_nor *snor;
	struct spi_nor_info *info;
	uint32_t index;
};

struct ufsnor_wp_instance {
	struct spi_nor *snor;
	struct spi_nor_info *info;
	struct spi_nor_wp_regions regions;
};

static struct ufsnor_options configs;
static struct ufsnor_instance snor_inst;

static const char usage[] =
	"Usage:\n"
	"    %s [dev=<dev>] [part=<partmodel>] [die=<id>] <subcommand> [option...]\n"
	"\n"
	"Global options:\n"
	"        dev  - Specify the device to be opened.\n"
	"               If not specified, last device recorded in config will be used.\n"
	"        part - Specify the part model to be used.\n"
	"               This will fail if flash ID mismatches.\n"
	"        die  - Specify the Die ID# to be operated.\n"
	"               If specified, only the selected Die will be used, and the memory\n"
	"               address will always start from zero.\n"
	"               If not specified, all Dies will be used for read/write/erase\n"
	"               using linear memory address and 0 will be used for rest\n"
	"               subcommands.\n"
	"               This is valid only if the flash has more than one Dies.\n"
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
	"\n"
	"    read <file> [<addr> [<size>]]\n"
	"        Read flash data to file.\n"
	"        file - The file path used to store flash data.\n"
	"        addr - The start flash address to read from.\n"
	"               Default is 0 if not specified.\n"
	"        size - The size to be read.\n"
	"               Default is the size from start address to end of flash.\n"
	"\n"
	"    dump sfdp\n"
	"        Dump SFDP data to stdout if exists.\n"
	"    dump [<addr> [<size>]]\n"
	"        Dump flash data to stdout.\n"
	"        addr - The start flash address to dumped.\n"
	"               Default is 0 if not specified.\n"
	"        size - The size to be dumped. Default is which to the end of page.\n"
	"\n"
	"    write [verify] <file> [<addr> [<size>]]\n"
	"    update [verify] <file> [<addr> [<size>]]\n"
	"        Write/update flash data from file.\n"
	"        If a block has only part of its data being written, the rest of its\n"
	"        data will be kept untouched by update subcommand while write subcommand\n"
	"        will not.\n"
	"        verify - Verify the data being written.\n"
	"        file   - The file to be written to flash.\n"
	"        addr   - The start flash address to be written to.\n"
	"                 Default is 0 if not specified.\n"
	"        size   - The size to be written. Default is the writable size from start\n"
	"                 address to end of flash."
	"                 If the file size is smaller that specified size, only available\n"
	"                 file data will be written.\n"
	"\n"
	"    erase chip|[<addr> [<size>]]\n"
	"        Erase flash range.\n"
	"        chip - Erase the whole chip.\n"
	"        addr - The start flash address to be erased.\n"
	"               Default is 0 if not specified.\n"
	"        size - The size to be erased. Default is the size from start address to\n"
	"               end of flash.\n"
	"               All blocks covered by the erase range will be erased.\n"
	"\n"
	"    uid\n"
	"        Read the Unique ID if supported.\n"
	"\n"
	"    reg list [<name>]\n"
	"        List non-volatile configuration registers if supported.\n"
	"    reg get [<name>] <field>\n"
	"        Get field of a non-volatile configuration register if supported.\n"
	"    reg set [<name>] <field> <val>\n"
	"        Set field of a non-volatile configuration register if supported.\n"
	"        name  - Specify the register name to be used. By default the first\n"
	"                register containing the field will be used.\n"
	"        field - Field name to be get/set.\n"
	"        val   - Field value to be set.\n"
	"\n"
	"    otp info\n"
	"        Display OTP region information.\n"
	"    otp [index=<index>] read <file>\n"
	"        Read OTP region into file.\n"
	"    otp [index=<index>] write [verify] <file>\n"
	"        Write data to OTP region.\n"
	"    otp [index=<index>] erase\n"
	"        Erase OTP region. This may not be supported.\n"
	"    otp [index=<index>] lock\n"
	"        Lock OTP region. The OTP region lock is permanent.\n"
	"        index  - Specify the OTP region index to be operated. This must be\n"
	"                 specified if more than one regions exist.\n"
	"        verify - Verify the data being written.\n"
	"        file   - The file to be read from/written to OTP region.\n"
	"\n"
	"    wp info\n"
	"        List write-protect region information.\n"
	"    wp set <start> <end>\n"
	"        Set write-protect region.\n"
	"        start - Start address of the write-protected region.\n"
	"        end   - End address of the write-protected region.\n";

static void show_usage(void)
{
	os_printf(usage, os_prog_name());
}

static int do_snor_list(void *priv, int argc, char *argv[])
{
	struct spi_nor_vendor_item *vendors;
	uint32_t i, n, vendor_width = 0;
	struct spi_nor_part_list *list;
	char *vendor = NULL, *part;
	ufprog_status ret;
	int argp;

	struct cmdarg_entry args[] = {
		CMDARG_STRING_OPT("vendor", vendor),
	};

	if (argc > 1 && !strcmp(argv[1], "vendors")) {
		ret = ufprog_spi_nor_list_vendors(&vendors, &n);
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

	ret = ufprog_spi_nor_list_parts(&list, vendor, part);
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

	ufprog_spi_nor_free_list(list);

	return 0;
}

static int do_snor_probe(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	char idstr[20], *model, *vendor;
	struct spi_nor_part_list *list;
	uint32_t i, j, mask;
	ufprog_status ret;

	mask = (1 << SPI_NOR_MAX_ERASE_INFO) - 1;

	for (i = 0; i < inst->info.num_erase_regions; i++)
		mask &= inst->info.erase_regions[i].erasesizes_mask;

	bin_to_hex_str(idstr, sizeof(idstr), inst->info.id.id, inst->info.id.len, true, true);
	os_printf("JEDEC ID:           %s\n", idstr);

	if (mask) {
		os_printf("Block/Sector size:  ");
		for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
			if (mask & BIT(i))
				os_printf("%uKB ", inst->info.erasesizes[i] >> 10);
		}
		os_printf("\n");
	}

	os_printf("Page size:          %uB\n", inst->info.page_size);
	os_printf("Max speed:          %uMHz\n", inst->info.max_speed / 1000000);
	os_printf("Protocol:           %s\n", inst->info.cmd_bw == 4 ? "QPI" : (inst->info.cmd_bw == 2 ? "DPI" : "SPI"));
	os_printf("Read I/O:           %u-%u-%u\n", spi_mem_io_info_cmd_bw(inst->info.read_io_info),
		  spi_mem_io_info_addr_bw(inst->info.read_io_info), spi_mem_io_info_data_bw(inst->info.read_io_info));
	os_printf("Write I/O:          %u-%u-%u\n", spi_mem_io_info_cmd_bw(inst->info.pp_io_info),
		  spi_mem_io_info_addr_bw(inst->info.pp_io_info), spi_mem_io_info_data_bw(inst->info.pp_io_info));

	if (inst->info.otp)
		os_printf("OTP:                %u * %uB\n", inst->info.otp->count, inst->info.otp->size);

	if (inst->info.num_erase_regions > 1) {
		os_printf("\n");
		os_printf("Sector Map:\n");

		for (i = 0; i < inst->info.num_erase_regions; i++) {
			os_printf("    %" PRIu64 "KB ( ", inst->info.erase_regions[i].size >> 10);

			for (j = 0; j < SPI_NOR_MAX_ERASE_INFO; j++) {
				if (inst->info.erase_regions[i].erasesizes_mask & BIT(j))
					os_printf("%uKB ", inst->info.erasesizes[i] >> 10);
			}

			os_printf(")\n");
		}
	}

	model = os_strdup(inst->info.model);
	if (!model) {
		os_fprintf(stderr, "No memory for model name\n");
		return 1;
	}

	vendor = os_strdup(inst->info.vendor);
	if (!vendor) {
		os_fprintf(stderr, "No memory for vendor name\n");
		free(model);
		return 1;
	}

	ret = ufprog_spi_nor_probe(inst->snor, &list, NULL);
	if (ret)
		goto cleanup;

	if (list->num <= 1)
		goto cleanup;

	os_printf("\n");
	os_printf("Other matched part(s):\n");

	for (i = 0; i < list->num; i++) {
		if (!strcasecmp(model, list->list[i].name) && !strcasecmp(vendor, list->list[i].vendor))
			continue;

		os_printf("    %s\t%s\n", list->list[i].vendor, list->list[i].name);
	}

	ufprog_spi_nor_free_list(list);

cleanup:
	free(model);
	free(vendor);

	return 0;
}

static int do_snor_read(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	uint64_t addr = 0, opsize, size;
	ufprog_status ret;
	char *file, *end;
	int exitcode = 1;
	file_mapping fm;
	void *p;

	if (argc == 1) {
		os_fprintf(stderr, "File not specified for storing data\n");
		return 1;
	}

	opsize = inst->info.size * (uint64_t)inst->die_count;

	file = argv[1];

	if (argc > 2) {
		addr = strtoull(argv[2], &end, 0);
		if (end == argv[2] || *end || addr == ULONG_MAX) {
			os_fprintf(stderr, "Start address is invalid\n");
			return 1;
		}

		if (addr >= opsize) {
			os_fprintf(stderr, "Start address (0x%" PRIx64 ") is bigger than flash max address (0x%" PRIx64 ")\n",
				   addr, opsize - 1);
			return 1;
		}
	}

	if (argc > 3) {
		size = strtoull(argv[3], &end, 0);
		if (end == argv[3] || *end || addr == ULONG_MAX) {
			os_fprintf(stderr, "Read size is invalid\n");
			return 1;
		}

		if (!size) {
			os_fprintf(stderr, "Nothing to read\n");
			return 1;
		}

		if (addr + size > opsize) {
			os_fprintf(stderr, "Read size exceeds flash size\n");
			return 1;
		}
	} else {
		size = opsize - addr;
	}

	ret = os_open_file_mapping(file, size, size, true, true, &fm);
	if (ret)
		return 1;

	if (!os_set_file_mapping_offset(fm, 0, &p))
		goto cleanup;

	ret = read_flash(inst, addr, size, p);
	if (!ret)
		exitcode = 0;

cleanup:
	os_close_file_mapping(fm);

	return exitcode;
}

static int do_snor_dump(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	uint64_t addr = 0, opsize, size;
	char *end;

	if (argc == 1) {
		os_fprintf(stderr, "Dump start address not specified\n");
		return 1;
	}

	if (!strcmp(argv[1], "sfdp")) {
		hexdump(inst->info.sfdp_data, inst->info.sfdp_size, 0, false);
		return 0;
	}

	opsize = inst->info.size * (uint64_t)inst->die_count;

	addr = strtoull(argv[1], &end, 0);
	if (end == argv[1] || *end || addr == ULONG_MAX) {
		os_fprintf(stderr, "Start address is invalid\n");
		return 1;
	}

	if (addr >= opsize) {
		os_fprintf(stderr, "Start address (0x%" PRIx64 " is bigger than flash max address (0x%" PRIx64 ")\n",
			   addr, opsize - 1);
		return 1;
	}

	if (argc > 2) {
		size = strtoull(argv[2], &end, 0);
		if (end == argv[2] || *end || addr == ULONG_MAX) {
			os_fprintf(stderr, "Dump size is invalid\n");
			return 1;
		}

		if (!size) {
			os_fprintf(stderr, "Nothing to dump\n");
			return 1;
		}

		if (addr + size > opsize) {
			size = opsize - addr;
			os_fprintf(stderr, "Dump size exceeds flash size. Adjusted to 0x%" PRIx64 "\n", size);
		}
	} else {
		size = opsize - addr;
		if (size > inst->info.page_size) {
			size = inst->info.page_size;
			size -= addr % inst->info.page_size;
		}
	}

	if (dump_flash(inst, addr, size))
		return 1;

	return 0;
}

static int do_snor_write_update(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	uint64_t addr = 0, opsize, maxsize, size;
	ufprog_bool verify;
	ufprog_status ret;
	char *file, *end;
	int exitcode = 1;
	file_mapping fm;
	int argp;
	void *p;

	struct cmdarg_entry args[] = {
		CMDARG_BOOL_OPT("verify", verify),
	};

	if (!parse_args(args, ARRAY_SIZE(args), argc, argv, &argp))
		return 1;

	if (argc == argp) {
		os_fprintf(stderr, "File not specified for writing data\n");
		return 1;
	}

	opsize = inst->info.size * (uint64_t)inst->die_count;

	file = argv[argp];

	if (argc > argp + 1) {
		addr = strtoull(argv[argp + 1], &end, 0);
		if (end == argv[argp + 1] || *end || addr == ULONG_MAX) {
			os_fprintf(stderr, "Start address is invalid\n");
			return 1;
		}

		if (addr >= opsize) {
			os_fprintf(stderr, "Start address (0x%" PRIx64 ") is bigger than flash max address (0x%" PRIx64 ")\n",
				   addr, opsize - 1);
			return 1;
		}
	}

	if (argc > argp + 2) {
		maxsize = strtoull(argv[argp + 2], &end, 0);
		if (end == argv[argp + 2] || *end || addr == ULONG_MAX) {
			os_fprintf(stderr, "Write size is invalid\n");
			return 1;
		}

		if (!maxsize) {
			os_fprintf(stderr, "Nothing to write\n");
			return 1;
		}

		if (addr + maxsize > opsize) {
			os_fprintf(stderr, "Write size exceeds flash size\n");
			return 1;
		}
	} else {
		maxsize = opsize - addr;
	}

	ret = os_open_file_mapping(file, 0, 0, false, false, &fm);
	if (ret)
		return 1;

	if (!os_set_file_mapping_offset(fm, 0, &p))
		goto cleanup;

	size = os_get_file_max_mapping_size(fm);
	if (size > maxsize)
		size = maxsize;

	ret = write_flash(inst, addr, size, p, !strcmp(argv[0], "update"), verify);
	if (!ret)
		exitcode = 0;

cleanup:
	os_close_file_mapping(fm);

	return exitcode;
}

static int do_snor_erase(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	uint64_t addr = 0, opsize, size;
	char *end;

	if (argc == 1) {
		os_fprintf(stderr, "Erase start address not specified\n");
		return 1;
	}

	opsize = inst->info.size * (uint64_t)inst->die_count;

	if (!strcmp(argv[1], "chip")) {
		size = opsize;
	} else {
		addr = strtoull(argv[1], &end, 0);
		if (end == argv[1] || *end || addr == ULONG_MAX) {
			os_fprintf(stderr, "Start address is invalid\n");
			return 1;
		}

		if (addr >= opsize) {
			os_fprintf(stderr, "Start address (0x%" PRIx64 ") is bigger than flash max address (0x%" PRIx64 ")\n",
				   addr, opsize - 1);
			return 1;
		}

		if (argc > 2) {
			size = strtoull(argv[2], &end, 0);
			if (end == argv[2] || *end || addr == ULONG_MAX) {
				os_fprintf(stderr, "Erase size is invalid\n");
				return 1;
			}

			if (!size) {
				os_fprintf(stderr, "Nothing to erase\n");
				return 1;
			}

			if (addr + size > opsize) {
				size = opsize - addr;
				os_fprintf(stderr, "Erase size exceeds flash size. Adjusted to 0x%" PRIx64 "\n", size);
			}
		} else {
			size = opsize - addr;
		}
	}

	if (erase_flash(inst, addr, size))
		return 1;

	return 0;
}

static int do_snor_uid(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	uint8_t uiddfl[DEFAULT_UID_MAX_LEN], *uid = NULL;
	ufprog_status ret;
	int exitcode = 1;
	uint32_t len, i;

	ret = ufprog_spi_nor_read_uid(inst->snor, NULL, &len);
	if (ret == UFP_UNSUPPORTED) {
		os_fprintf(stderr, "Unique ID is not supported by this flash chip\n");
		return 1;
	}

	if (len <= sizeof(uiddfl))
		uid = uiddfl;

	if (!uid) {
		uid = malloc(len);
		if (!uid) {
			logm_err("No memory for Unique ID\n");
			return 1;
		}
	}

	ret = ufprog_spi_nor_read_uid(inst->snor, uid, NULL);
	if (ret) {
		os_fprintf(stderr, "Failed to read Unique ID\n");
		goto out;
	}

	os_printf("Unique ID: ");

	for (i = 0; i < len; i++)
		os_printf("%02x", uid[i]);

	os_printf("\n");

	exitcode = 0;

out:
	if (uid != uiddfl)
		free(uid);

	return exitcode;
}

static const char *snor_reg_field_get_value_name(const struct spi_nor_reg_field_values *values, uint32_t val)
{
	uint32_t i;

	if (!values)
		return NULL;

	for (i = 0; i < values->num; i++) {
		if (values->items[i].value == val)
			return values->items[i].name;
	}

	return NULL;
}

static ufprog_status snor_print_reg_fields(struct ufsnor_instance *inst, const struct spi_nor_reg_def *reg)
{
	uint32_t i, j, val, fval;
	const char *fname;
	ufprog_status ret;

	os_printf("Register '%s' (%s): ", reg->name, reg->desc);

	ret = ufprog_spi_nor_read_reg(inst->snor, reg->access, &val);
	if (ret) {
		os_printf("\n");
		os_fprintf(stderr, "Failed to read register '%s'\n", reg->name);
		return ret;
	}

	os_printf("0x%02X\n", val);

	for (i = 0; i < reg->nfields; i++) {
		fval = (val >> reg->fields[i].shift) & reg->fields[i].mask;
		fname = snor_reg_field_get_value_name(reg->fields[i].values, fval);

		if (fname)
			os_printf("    %s (%s) = %u (%s)\n", reg->fields[i].name, reg->fields[i].desc, fval, fname);
		else
			os_printf("    %s (%s) = %u\n", reg->fields[i].name, reg->fields[i].desc, fval);

		if (reg->fields[i].values) {
			os_printf("      Available values:\n");

			for (j = 0; j < reg->fields[i].values->num; j++) {
				os_printf("        %u - %s\n", reg->fields[i].values->items[j].value,
					  reg->fields[i].values->items[j].name);
			}
		}
	}

	return UFP_OK;
}

static int do_snor_reg_list(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	const struct spi_nor_reg_def *reg;
	char *regname = NULL;
	ufprog_status ret;
	int exitcode = 0;
	uint32_t i, n = 0;

	if (argc > 1 && *argv[1])
		regname = argv[1];

	for (i = 0; i < inst->info.regs->num; i++) {
		reg = inst->info.regs->regs[i];

		if (regname) {
			if (strcasecmp(regname, reg->name))
				continue;
		}

		ret = snor_print_reg_fields(inst, reg);
		if (ret)
			exitcode = 1;

		n++;
		os_printf("\n");
	}

	if (!n && regname) {
		os_fprintf(stderr, "No register named '%s'\n", regname);
		exitcode = 1;
	}

	return exitcode;
}

static const struct spi_nor_reg_def *snor_reg_find(struct ufsnor_instance *inst, const char *name)
{
	const struct spi_nor_reg_def *reg;
	uint32_t i;

	for (i = 0; i < inst->info.regs->num; i++) {
		reg = inst->info.regs->regs[i];

		if (!strcasecmp(name, reg->name))
			return reg;
	}

	return NULL;
}

static const struct spi_nor_reg_field_item *snor_reg_find_field(const struct spi_nor_reg_def *reg, const char *name)
{
	const struct spi_nor_reg_field_item *field;
	uint32_t i;

	for (i = 0; i < reg->nfields; i++) {
		field = &reg->fields[i];

		if (!strcasecmp(name, field->name))
			return field;
	}

	return NULL;
}

static const struct spi_nor_reg_field_item *snor_regs_find_field(struct ufsnor_instance *inst,
								 const struct spi_nor_reg_def **retreg,
								 const char *name)
{
	const struct spi_nor_reg_field_item *field;
	const struct spi_nor_reg_def *reg;
	uint32_t i, j;

	for (i = 0; i < inst->info.regs->num; i++) {
		reg = inst->info.regs->regs[i];

		for (j = 0; j < reg->nfields; j++) {
			field = &reg->fields[j];

			if (!strcasecmp(name, field->name)) {
				*(retreg) = reg;
				return field;
			}
		}
	}

	return NULL;
}

static int do_snor_reg_get(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	const struct spi_nor_reg_field_item *field = NULL;
	const struct spi_nor_reg_def *reg = NULL;
	const char *regname = NULL, *fieldname;
	uint32_t i, val, fval;
	const char *fname;
	ufprog_status ret;

	if (argc == 1) {
		os_fprintf(stderr, "Field name not specified\n");
		return 1;
	}

	if (argc == 2) {
		fieldname = argv[1];
	} else {
		if (*argv[1])
			regname = argv[1];
		fieldname = argv[2];
	}

	if (!regname) {
		field = snor_regs_find_field(inst, &reg, fieldname);
	} else {
		reg = snor_reg_find(inst, regname);
		if (reg)
			field = snor_reg_find_field(reg, fieldname);
	}

	if (!field) {
		if (regname)
			os_fprintf(stderr, "Register '%s' does not have a field named '%s'\n", regname, fieldname);
		else
			os_fprintf(stderr, "Field named '%s' not found\n", fieldname);

		return 1;
	}

	ret = ufprog_spi_nor_read_reg(inst->snor, reg->access, &val);
	if (ret) {
		os_fprintf(stderr, "Failed to read register '%s'\n", reg->name);
		return 1;
	}

	fval = (val >> field->shift) & field->mask;
	fname = snor_reg_field_get_value_name(field->values, fval);

	if (fname)
		os_printf("%s: %s (%s) = %u (%s)\n", reg->name, field->name, field->desc, fval, fname);
	else
		os_printf("%s: %s (%s) = %u\n", reg->name, field->name, field->desc, fval);

	if (field->values) {
		os_printf("    Available values:\n");

		for (i = 0; i < field->values->num; i++) {
			os_printf("      %u - %s\n", field->values->items[i].value,
				  field->values->items[i].name);
		}
	}

	return 0;
}

static int do_snor_reg_set(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	const char *regname = NULL, *fieldname, *fieldval, *fname;
	const struct spi_nor_reg_field_item *field = NULL;
	const struct spi_nor_reg_def *reg = NULL;
	uint32_t val, fmask, fval, fval_sft;
	ufprog_status ret;
	char *end;

	if (argc <= 2) {
		os_fprintf(stderr, "Missing field name/value\n");
		return 1;
	}

	if (argc == 3) {
		fieldname = argv[1];
		fieldval = argv[2];
	} else {
		if (*argv[1])
			regname = argv[1];
		fieldname = argv[2];
		fieldval = argv[3];
	}

	if (!regname) {
		field = snor_regs_find_field(inst, &reg, fieldname);
	} else {
		reg = snor_reg_find(inst, regname);
		if (reg)
			field = snor_reg_find_field(reg, fieldname);
	}

	if (!field) {
		if (regname)
			os_fprintf(stderr, "Register '%s' does not have a field named '%s'\n", regname, fieldname);
		else
			os_fprintf(stderr, "Field named '%s' not found\n", fieldname);

		return 1;
	}

	fval = strtoul(fieldval, &end, 0);
	if (fieldval == end || *end) {
		os_fprintf(stderr, "Field value '%s' is invalid\n", fieldval);
		return 1;
	}

	if (fval > field->mask) {
		os_fprintf(stderr, "Field value %u is out of range (max %u)\n", fval, field->mask);
		return 1;
	}

	fmask = field->mask << field->shift;
	fval_sft = fval << field->shift;

	ret = ufprog_spi_nor_update_reg(inst->snor, reg->access, fmask, fval_sft);
	if (ret) {
		os_fprintf(stderr, "Failed to update field '%s' of register '%s'\n", field->name, reg->name);
		return 1;
	}

	ret = ufprog_spi_nor_read_reg(inst->snor, reg->access, &val);
	if (ret) {
		os_fprintf(stderr, "Failed to read register '%s'\n", reg->name);
		return 1;
	}

	if ((val & fmask) != fval_sft) {
		os_fprintf(stderr, "Failed to update field '%s' of register '%s'. Expect %xh, got %xh\n",
			   field->name, reg->name, fval, (val & fmask) >> field->shift);
		return 1;
	}

	fname = snor_reg_field_get_value_name(field->values, fval);

	if (fname)
		os_printf("%s: %s (%s) has been set to %u (%s)\n", reg->name, field->name, field->desc, fval, fname);
	else
		os_printf("%s: %s (%s) has been set to %u\n", reg->name, field->name, field->desc, fval);

	return 0;
}

static const struct subcmd_entry reg_cmds[] = {
	SUBCMD("list", do_snor_reg_list),
	SUBCMD("get", do_snor_reg_get),
	SUBCMD("set", do_snor_reg_set),
};

static int do_snor_reg(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	ufprog_status ret;
	int exitcode;

	if (!inst->info.regs) {
		os_fprintf(stderr, "Register not defined for this flash chip\n");
		return 1;
	}

	if (argc == 1) {
		os_fprintf(stderr, "Missing sub-subcommand for reg subcommand\n");
		return 1;
	}

	ret = dispatch_subcmd(reg_cmds, ARRAY_SIZE(reg_cmds), priv, argc - 1, argv + 1, &exitcode);
	if (ret) {
		os_fprintf(stderr, "'%s' is not supported by reg subcommand\n", argv[1]);
		return 1;
	}

	return exitcode;
}

static int do_snor_otp_info(void *priv, int argc, char *argv[])
{
	struct ufsnor_otp_instance *inst = priv;
	ufprog_bool locked;
	ufprog_status ret;
	uint32_t i;

	os_printf("OTP information:\n");
	os_printf("    Region count: %u\n", inst->info->otp->count);
	os_printf("    Region size:  %uB\n", inst->info->otp->size);
	os_printf("    Start index:  %u\n", inst->info->otp->start_index);
	os_printf("    Erasable:     %s\n", inst->info->otp_erasable ? "Yes" : "No");
	os_printf("\n");

	for (i = 0; i < inst->info->otp->count; i++) {
		ret = ufprog_spi_nor_otp_locked(inst->snor, inst->info->otp->start_index + i, &locked);
		if (ret) {
			os_fprintf(stderr, "Failed to get lock status of OTP region %u\n",
				   inst->info->otp->start_index + i);
			continue;
		}

		os_printf("OTP region %u is %s\n", inst->info->otp->start_index + i, locked ? "locked" : "not locked");
	}

	return 0;
}

static int do_snor_otp_read(void *priv, int argc, char *argv[])
{
	struct ufsnor_otp_instance *inst = priv;
	ufprog_status ret;
	int exitcode = 1;
	file_mapping fm;
	char *file;
	void *p;

	if (argc == 1) {
		os_fprintf(stderr, "File to store OTP data must be specified\n");
		return 1;
	}

	file = argv[1];

	ret = os_open_file_mapping(file, inst->info->otp->size, inst->info->otp->size, true, true, &fm);
	if (ret)
		return 1;

	if (!os_set_file_mapping_offset(fm, 0, &p))
		goto cleanup;

	ret = ufprog_spi_nor_otp_read(inst->snor, inst->index, 0, inst->info->otp->size, p);
	if (ret) {
		os_fprintf(stderr, "Failed to read OTP region %u\n", inst->index);
		goto cleanup;
	}

	os_printf("OTP region %u has been read to '%s'\n", inst->index, file);
	exitcode = 0;

cleanup:
	os_close_file_mapping(fm);

	return exitcode;
}

static int do_snor_otp_write(void *priv, int argc, char *argv[])
{
	struct ufsnor_otp_instance *inst = priv;
	ufprog_bool locked, cmpret, verify = false;
	int argp, exitcode = 1;
	size_t cmppos, fsize;
	ufprog_status ret;
	uint8_t *vbuf, *p;
	file_mapping fm;
	char *file;

	struct cmdarg_entry args[] = {
		CMDARG_BOOL_OPT("verify", verify),
	};

	ret = ufprog_spi_nor_otp_locked(inst->snor, inst->index, &locked);
	if (ret) {
		os_fprintf(stderr, "Failed to get lock status of OTP region %u\n", inst->index);
		return 1;
	}

	if (locked) {
		os_fprintf(stderr, "OTP region %u was permanently locked\n", inst->index);
		return 1;
	}

	if (!parse_args(args, ARRAY_SIZE(args), argc, argv, &argp))
		return 1;

	if (argp == argc) {
		os_fprintf(stderr, "File to store OTP data must be specified\n");
		return 1;
	}

	file = argv[argp];

	ret = os_open_file_mapping(file, 0, 0, false, false, &fm);
	if (ret)
		return 1;

	fsize = os_get_file_max_mapping_size(fm);
	if (fsize != inst->info->otp->size) {
		os_fprintf(stderr, "File size is not equal to OTP region size\n");
		goto cleanup;
	}

	if (!os_set_file_mapping_offset(fm, 0, (void **)&p))
		goto cleanup;

	ret = ufprog_spi_nor_otp_write(inst->snor, inst->index, 0, inst->info->otp->size, p);
	if (ret) {
		os_fprintf(stderr, "Failed to write to OTP region %u\n", inst->index);
		goto cleanup;
	}

	exitcode = 0;

	if (verify) {
		vbuf = malloc(inst->info->otp->size);
		if (!vbuf) {
			os_fprintf(stderr, "No memory OTP region verification\n");
			goto cleanup;
		}

		ret = ufprog_spi_nor_otp_read(inst->snor, inst->index, 0, inst->info->otp->size, vbuf);
		if (ret) {
			os_fprintf(stderr, "Failed to read OTP region %u\n", inst->index);
			goto cleanup2;
		}

		cmpret = bufdiff(p, vbuf, inst->info->otp->size, &cmppos);
		if (!cmpret) {
			os_fprintf(stderr, "Data at 0x%zx are different: expect 0x%02x, got 0x%02x\n",
				   cmppos, p[cmppos], vbuf[cmppos]);
			exitcode = 1;
		}

	cleanup2:
		free(vbuf);
	}

	if (!exitcode)
		os_printf("OTP region %u has been written with data from '%s'\n", inst->index, file);
	else
		os_printf("Failed to write to OTP region %u\n", inst->index);

cleanup:
	os_close_file_mapping(fm);

	return exitcode;
}

static int do_snor_otp_erase(void *priv, int argc, char *argv[])
{
	struct ufsnor_otp_instance *inst = priv;
	ufprog_bool locked;
	ufprog_status ret;

	ret = ufprog_spi_nor_otp_locked(inst->snor, inst->index, &locked);
	if (ret) {
		os_fprintf(stderr, "Failed to get lock status of OTP region %u\n", inst->index);
		return 1;
	}

	if (locked) {
		os_fprintf(stderr, "OTP region %u was permanently locked\n", inst->index);
		return 1;
	}

	if (!inst->info->otp_erasable) {
		os_fprintf(stderr, "Erasing OTP region is not supported\n");
		return 1;
	}

	ret = ufprog_spi_nor_otp_erase(inst->snor, inst->index);
	if (ret) {
		os_fprintf(stderr, "Failed to erase OTP region %u\n", inst->index);
		return 1;
	}

	os_printf("OTP region %u has been erased\n", inst->index);

	return 0;
}

static int do_snor_otp_lock(void *priv, int argc, char *argv[])
{
	struct ufsnor_otp_instance *inst = priv;
	bool confirmed = false;
	ufprog_bool locked;
	ufprog_status ret;
	char *confirm_str;

	ret = ufprog_spi_nor_otp_locked(inst->snor, inst->index, &locked);
	if (ret) {
		os_fprintf(stderr, "Failed to get lock status of OTP region %u\n", inst->index);
		return 1;
	}

	if (locked) {
		os_fprintf(stderr, "OTP region %u has already been locked\n", inst->index);
		return 1;
	}

	os_printf("Locking OTP region %u is irreversible and its data will be read-only forever.\n", inst->index);
	os_printf("Are you sure you want to lock it? Type \"confirm\" with enter to continue.\n");

	confirm_str = os_getline_alloc(stdin);
	if (!confirm_str) {
		os_fprintf(stderr, "Failed to read from stdin\n");
		return 1;
	}

	if (!strncmp(confirm_str, "confirm", 7))
		confirmed = true;

	free(confirm_str);

	if (!confirmed) {
		os_fprintf(stderr, "Error: OTP locking cancelled\n");
		return 0;
	}

	ret = ufprog_spi_nor_otp_lock(inst->snor, inst->index);
	if (ret) {
		os_fprintf(stderr, "Failed to lock OTP region %u\n", inst->index);
		return 1;
	}

	os_printf("OTP region %u is permanently locked\n", inst->index);

	return 0;
}

static const struct subcmd_entry otp_cmds[] = {
	SUBCMD("info", do_snor_otp_info),
	SUBCMD("read", do_snor_otp_read),
	SUBCMD("write", do_snor_otp_write),
	SUBCMD("erase", do_snor_otp_erase),
	SUBCMD("lock", do_snor_otp_lock),
};

static int do_snor_otp(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	struct ufsnor_otp_instance otp_inst;
	ufprog_bool index_set;
	int exitcode, argp;
	ufprog_status ret;
	uint32_t index;

	if (!inst->info.otp) {
		os_fprintf(stderr, "OTP region not defined for this flash chip\n");
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
		if (index < inst->info.otp->start_index ||
		    index >= inst->info.otp->start_index + inst->info.otp->count) {
			os_fprintf(stderr, "OTP region index %u is invalid\n", index);
			return 1;
		}
	} else {
		if (inst->info.otp->count > 1 && strcmp(argv[argp], "info")) {
			os_fprintf(stderr, "OTP region index must be specified\n");
			return 1;
		}

		index = inst->info.otp->start_index;
	}

	otp_inst.index = index;
	otp_inst.snor = inst->snor;
	otp_inst.info = &inst->info;

	ret = dispatch_subcmd(otp_cmds, ARRAY_SIZE(otp_cmds), &otp_inst, argc - argp, argv + argp, &exitcode);
	if (ret) {
		os_fprintf(stderr, "'%s' is not supported by otp subcommand\n", argv[1]);
		return 1;
	}

	return exitcode;
}

static void snor_print_wp_region(struct ufsnor_wp_instance *inst, const struct spi_nor_wp_region *rg,
				 const struct spi_nor_wp_region *rgactive)
{
	uint32_t width = 6;
	bool cmp = false;
	const char *unit;
	uint64_t size;

	if (inst->info->size > 0x1000000)
		width = 8;

	if (!rg->size)
		os_printf("    %0*Xh - %0*Xh", width, 0, width, 0);
	else if (rg->size == inst->info->size)
		os_printf("    %0*Xh - %0*" PRIX64 "h", width, 0, width, inst->info->size - 1);
	else
		os_printf("    %0*" PRIX64 "h - %0*" PRIX64 "h", width, rg->base, width, rg->base + rg->size - 1);

	if (!rg->size) {
		os_printf(" (NONE)");
	} else if (rg->size == inst->info->size) {
		os_printf(" (ALL)");
	} else {
		if (rg->size <= inst->info->size / 2) {
			if (!rg->base)
				os_printf(" (Lower ");
			else
				os_printf(" (Upper ");

			size = rg->size;
		} else {
			if (rg->base)
				os_printf(" (Lower ");
			else
				os_printf(" (Upper ");

			size = inst->info->size - rg->size;
			cmp = true;
		}

		if (size < SZ_1K) {
			unit = "";
		} else if (size < SZ_1M) {
			size >>= 10;
			unit = "K";
		} else {
			size >>= 20;
			unit = "M";
		}

		os_printf("%" PRIu64 "%sB", size, unit);

		if (cmp)
			os_printf(" CMP");

		os_printf(")");
	}

	if (rg->base == rgactive->base && rg->size == rgactive->size)
		os_printf(" [Active]");

	os_printf("\n");
}

static int do_snor_wp_info(void *priv, int argc, char *argv[])
{
	struct ufsnor_wp_instance *inst = priv;
	struct spi_nor_wp_region rg;
	ufprog_status ret;
	uint32_t i;

	ret = ufprog_spi_nor_get_wp_region(inst->snor, &rg);
	if (ret) {
		os_fprintf(stderr, "Failed to get current wp region\n");
		return 1;
	}

	os_printf("Supported write-protect regions:\n");

	for (i = 0; i < inst->regions.num; i++)
		snor_print_wp_region(inst, &inst->regions.region[i], &rg);

	return 0;
}

static int do_snor_wp_set(void *priv, int argc, char *argv[])
{
	struct ufsnor_wp_instance *inst = priv;
	struct spi_nor_wp_region rg;
	ufprog_status ret;
	uint64_t endaddr;
	char *end;

	if (argc < 3) {
		os_fprintf(stderr, "Missing write-protect base/size\n");
		return 1;
	}

	rg.base = strtoull(argv[1], &end, 0);
	if (end == argv[1] || *end || rg.base == ULONG_MAX) {
		os_fprintf(stderr, "Start address is invalid\n");
		return 1;
	}

	if (rg.base >= inst->info->size) {
		os_fprintf(stderr, "Start address (0x%" PRIx64 ") is bigger than flash max address (0x%" PRIx64 ")\n",
			   rg.base, inst->info->size - 1);
		return 1;
	}

	endaddr = strtoull(argv[2], &end, 0);
	if (end == argv[2] || *end || endaddr == ULONG_MAX) {
		os_fprintf(stderr, "End address is invalid\n");
		return 1;
	}

	if (endaddr >= inst->info->size) {
		os_fprintf(stderr, "End address (0x%" PRIx64 ") is bigger than flash max address (0x%" PRIx64 ")\n",
			   endaddr, inst->info->size - 1);
		return 1;
	}

	if (endaddr < rg.base) {
		os_fprintf(stderr, "End address (0x%" PRIx64 ") is smaller than start address (0x%" PRIx64 ")\n",
			   endaddr, rg.base);
		return 1;
	}

	if (endaddr > rg.base)
		rg.size = endaddr - rg.base + 1;
	else
		rg.size = 0;

	ret = ufprog_spi_nor_set_wp_region(inst->snor, &rg);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			os_fprintf(stderr, "Specified write-protect region is not supported\n");
		else
			os_fprintf(stderr, "Failed to set write-protect region\n");

		return 1;
	}

	os_printf("Write-protect region is set to:\n");

	snor_print_wp_region(inst, &rg, &rg);

	return 0;
}

static const struct subcmd_entry wp_cmds[] = {
	SUBCMD("info", do_snor_wp_info),
	SUBCMD("set", do_snor_wp_set),
};

static int do_snor_wp(void *priv, int argc, char *argv[])
{
	struct ufsnor_instance *inst = priv;
	struct ufsnor_wp_instance wp_inst;
	ufprog_status ret;
	int exitcode;

	if (argc == 1) {
		os_fprintf(stderr, "Missing sub-subcommand for wp subcommand\n");
		return 1;
	}

	ret = ufprog_spi_nor_get_wp_region_list(inst->snor, &wp_inst.regions);
	if (ret) {
		if (ret == UFP_UNSUPPORTED)
			os_fprintf(stderr, "Write-protect regions not defined for this flash chip\n");
		else
			os_fprintf(stderr, "Failed to get write-protect regions\n");

		return 1;
	}

	wp_inst.snor = inst->snor;
	wp_inst.info = &inst->info;

	ret = dispatch_subcmd(wp_cmds, ARRAY_SIZE(wp_cmds), &wp_inst, argc - 1, argv + 1, &exitcode);
	if (ret) {
		os_fprintf(stderr, "'%s' is not supported by wp subcommand\n", argv[1]);
		return 1;
	}

	return exitcode;
}

static const struct subcmd_entry cmds[] = {
	SUBCMD("list", do_snor_list),
	SUBCMD("probe", do_snor_probe),
	SUBCMD("read", do_snor_read),
	SUBCMD("dump", do_snor_dump),
	SUBCMD("write", do_snor_write_update),
	SUBCMD("update", do_snor_write_update),
	SUBCMD("erase", do_snor_erase),
	SUBCMD("uid", do_snor_uid),
	SUBCMD("reg", do_snor_reg),
	SUBCMD("otp", do_snor_otp),
	SUBCMD("wp", do_snor_wp),
};

static int ufprog_main(int argc, char *argv[])
{
	char *device_name = NULL, *part = NULL;
	struct ufsnor_options nopt;
	const char *last_devname;
	bool allow_fail = false;
	ufprog_bool die_set;
	int exitcode, argp;
	ufprog_status ret;
	uint32_t die = 0;
	char *devname;

	struct cmdarg_entry args[] = {
		CMDARG_STRING_OPT("dev", device_name),
		CMDARG_STRING_OPT("part", part),
		CMDARG_U32_OPT_SET("die", die, die_set),
	};

	set_os_default_log_print();
	os_init();

	os_printf("Universal flash programmer for SPI-NOR %s %s\n", UFP_VERSION,
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

	ufprog_spi_nor_load_ext_id_file();

	ret = load_config(&configs, device_name);
	if (ret)
		return 1;

	if (!device_name)
		devname = configs.last_device;
	else
		devname = device_name;

	allow_fail = !strcmp(argv[argp], "list");

	ret = open_device(devname, part, configs.max_speed, &snor_inst, allow_fail);
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

	if (snor_inst.snor) {
		if (!die_set) {
			snor_inst.die_start = 0;
			snor_inst.die_count = snor_inst.info.ndies;
		} else {
			if (die >= snor_inst.info.ndies) {
				if (snor_inst.info.ndies > 1) {
					os_fprintf(stderr, "Die ID# %u is invalid. Only %u available\n", die,
						   snor_inst.info.ndies);
				} else {
					os_fprintf(stderr, "Die ID# %u is invalid. Only one available\n", die);
				}
			}

			snor_inst.die_start = die;
			snor_inst.die_count = 1;
		}

		ret = ufprog_spi_nor_select_die(snor_inst.snor, snor_inst.die_start);
		if (ret) {
			os_fprintf(stderr, "Failed to select Die %u\n", snor_inst.die_start);
			exitcode = 1;
			goto cleanup;
		}

		if (die_set)
			os_printf("Selected Die %u\n", die);
	}

	ret = dispatch_subcmd(cmds, ARRAY_SIZE(cmds), &snor_inst, argc - argp, argv + argp, &exitcode);
	if (ret) {
		os_fprintf(stderr, "'%s' is not a supported subcommand\n", argv[1]);
		os_fprintf(stderr, "\n");
		show_usage();
		exitcode = 1;
	}

cleanup:
	if (snor_inst.snor) {
		ufprog_spi_nor_detach(snor_inst.snor, true);
		ufprog_spi_nor_destroy(snor_inst.snor);
	}

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
