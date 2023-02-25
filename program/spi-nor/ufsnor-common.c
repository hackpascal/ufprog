// SPDX-License-Identifier: GPL-2.0-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash programmer common part
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ufprog/misc.h>
#include <ufprog/sizes.h>
#include <ufprog/osdef.h>
#include <ufprog/config.h>
#include <ufprog/progbar.h>
#include <ufprog/hexdump.h>
#include <ufprog/buffdiff.h>
#include "ufsnor-common.h"

struct snor_update_backup_info {
	uint64_t addr;
	uint64_t size;
	uint8_t *data;
};

/* XXX: not thread-safe */
static uint8_t *verify_buffer;

bool parse_args(struct cmdarg_entry *entries, uint32_t count, int argc, char *argv[], int *next_argc)
{
	ufprog_status ret;
	uint32_t erridx;
	int nargc;

	ret = cmdarg_parse(entries, count, argc - 1, argv + 1, &nargc, &erridx, NULL);
	if (ret) {
		if (ret == UFP_CMDARG_MISSING_VALUE)
			os_fprintf(stderr, "Argument '%s' is missing value\n", entries[erridx].name);
		else if (ret == UFP_CMDARG_INVALID_TYPE)
			os_fprintf(stderr, "The value of argument '%s' is invalid\n", entries[erridx].name);
		else
			os_fprintf(stderr, "The value of argument '%s' is invalid\n", entries[erridx].name);

		return false;
	}

	*next_argc = nargc + 1;

	return true;
}

ufprog_status load_config(struct ufsnor_options *retcfg, const char *curr_device)
{
	struct json_object *jroot, *device_cfgs, *device_cfg;
	const char *last_device;
	ufprog_status ret;

	ret = json_open_config(os_prog_name(), &jroot);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST) {
			retcfg->last_device = NULL;
			retcfg->log_level = DEFAULT_LOG_LEVEL;
			retcfg->global_max_speed = UFSNOR_MAX_SPEED;
			retcfg->max_speed = UFSNOR_MAX_SPEED;
			return UFP_OK;
		}

		os_fprintf(stderr, "Failed to load config file\n");
		return ret;
	}

	retcfg->last_device = NULL;

	ret = json_read_str(jroot, "last-device", &last_device, NULL);
	if (ret == UFP_JSON_TYPE_INVALID) {
		os_fprintf(stderr, "'/last-device' in config file is invalid\n");
		ret = UFP_FAIL;
		goto cleanup;
	}

	if (last_device && *last_device) {
		retcfg->last_device = os_strdup(last_device);
		if (!retcfg->last_device) {
			os_fprintf(stderr, "Failed to duplicate '/last-device' from config\n");
			ret = UFP_FAIL;
			goto cleanup;
		}
	}

	ret = json_read_uint32(jroot, "log-level", &retcfg->log_level, DEFAULT_LOG_LEVEL);
	if (ret == UFP_JSON_TYPE_INVALID) {
		os_fprintf(stderr, "'/log-level' in config file is invalid\n");
		ret = UFP_FAIL;
		goto cleanup;
	}

	if (retcfg->log_level >= __MAX_LOG_LEVEL) {
		os_fprintf(stderr, "Log level specified in config file is invalid\n");
		ret = UFP_JSON_DATA_INVALID;
		goto cleanup;
	}

	ret = json_read_uint32(jroot, "max-speed-hz", &retcfg->global_max_speed, UFSNOR_MAX_SPEED);
	if (ret == UFP_JSON_TYPE_INVALID) {
		os_fprintf(stderr, "'/max-speed-hz' in config file is invalid\n");
		ret = UFP_FAIL;
		goto cleanup;
	}

	retcfg->max_speed = retcfg->global_max_speed;

	if (!curr_device)
		curr_device = retcfg->last_device;

	if (!curr_device) {
		ret = UFP_OK;
		goto cleanup;
	}

	ret = json_read_obj(jroot, "device-configs", &device_cfgs);
	if (ret == UFP_JSON_TYPE_INVALID) {
		os_fprintf(stderr, "'/device-configs' in config file is invalid\n");
		ret = UFP_FAIL;
		goto cleanup;
	}

	if (!device_cfgs) {
		ret = UFP_OK;
		goto cleanup;
	}

	ret = json_read_obj(device_cfgs, curr_device, &device_cfg);
	if (ret == UFP_JSON_TYPE_INVALID) {
		os_fprintf(stderr, "'/device-configs/%s' in config file is invalid\n", curr_device);
		ret = UFP_FAIL;
		goto cleanup;
	}

	if (!device_cfg) {
		ret = UFP_OK;
		goto cleanup;
	}

	ret = json_read_uint32(device_cfg, "max-speed-hz", &retcfg->max_speed, retcfg->max_speed);
	if (ret == UFP_JSON_TYPE_INVALID) {
		os_fprintf(stderr, "'/device-configs/%s/max-speed-hz' in config file is invalid\n", curr_device);
		ret = UFP_FAIL;
		goto cleanup;
	}

	ret = UFP_OK;

cleanup:
	if (ret) {
		free(retcfg->last_device);
		retcfg->last_device = NULL;
	}

	json_free(jroot);

	return ret;
}

ufprog_status save_config(const struct ufsnor_options *cfg)
{
	struct json_object *jroot, *device_cfgs, *device_cfg;
	ufprog_status ret;

	ret = json_open_config(os_prog_name(), &jroot);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST) {
			ret = json_from_str("{}", &jroot);
			if (ret) {
				os_fprintf(stderr, "No memory to create json object\n");
				return ret;
			}
		} else {
			os_fprintf(stderr, "Failed to load config file\n");
			return ret;
		}
	}

	ret = json_set_str(jroot, "last-device", cfg->last_device, -1);
	if (ret) {
		os_fprintf(stderr, "Failed to set '/last-device' in config\n");
		ret = UFP_FAIL;
		goto cleanup;
	}

	ret = json_read_obj(jroot, "device-configs", &device_cfgs);
	if (ret == UFP_JSON_TYPE_INVALID) {
		os_fprintf(stderr, "'/device-configs' in config is invalid\n");
		ret = UFP_FAIL;
		goto cleanup;
	}

	if (!device_cfgs) {
		ret = json_create_obj(&device_cfgs);
		if (ret) {
			os_fprintf(stderr, "Failed to create '/device-configs'\n");
			ret = UFP_FAIL;
			goto cleanup;
		}

		ret = json_add_obj(jroot, "device-configs", device_cfgs);
		if (ret) {
			os_fprintf(stderr, "Failed to add '/device-configs' in to config\n");
			json_put_obj(device_cfgs);
			ret = UFP_FAIL;
			goto cleanup;
		}
	}

	ret = json_read_obj(device_cfgs, cfg->last_device, &device_cfg);
	if (ret == UFP_JSON_TYPE_INVALID) {
		os_fprintf(stderr, "'/device-configs/%s' in config file is invalid\n", cfg->last_device);
		ret = UFP_FAIL;
		goto cleanup;
	}

	if (!device_cfg) {
		ret = json_create_obj(&device_cfg);
		if (ret) {
			os_fprintf(stderr, "Failed to create '/device-configs/%s'\n", cfg->last_device);
			ret = UFP_FAIL;
			goto cleanup;
		}

		ret = json_add_obj(device_cfgs, cfg->last_device, device_cfg);
		if (ret) {
			os_fprintf(stderr, "Failed to add '/device-configs/%s' in to config\n", cfg->last_device);
			json_put_obj(device_cfg);
			ret = UFP_FAIL;
			goto cleanup;
		}
	}

	ret = json_set_uint(device_cfg, "max-speed-hz", cfg->max_speed);
	if (ret) {
		os_fprintf(stderr, "Failed to set '/device-configs/%s/max-speed-hz' in config\n", cfg->last_device);
		ret = UFP_FAIL;
		goto cleanup;
	}

	ret = json_save_config(os_prog_name(), jroot);
	if (ret) {
		os_fprintf(stderr, "Failed to save config file\n");
		return ret;
	}

cleanup:
	json_free(jroot);

	return ret;
}

ufprog_status open_device(const char *device_name, const char *part, uint32_t max_speed,
			  struct ufsnor_instance *retinst, bool allow_fail)
{
	ufprog_status ret;
	uint64_t size;
	char *unit;

	if (!device_name) {
		os_fprintf(stderr, "Device name not specified\n");
		goto errout;
	}

	retinst->snor = ufprog_spi_nor_create();
	if (!retinst->snor) {
		os_fprintf(stderr, "Failed to create spi-nor instance\n");
		goto errout;
	}

	if (!max_speed)
		max_speed = UFSNOR_MAX_SPEED;

	ufprog_spi_nor_set_speed_limit(retinst->snor, max_speed);

	ret = ufprog_spi_open_device(device_name, false, &retinst->spi);
	if (ret) {
		os_fprintf(stderr, "Failed to open device '%s'\n", device_name);
		ufprog_spi_nor_destroy(retinst->snor);
		goto errout;
	}

	os_printf("\n");

	ret = ufprog_spi_nor_attach(retinst->snor, retinst->spi);
	if (!retinst->snor) {
		os_fprintf(stderr, "Failed to attach spi interface to spi-nor instance\n");
		ufprog_spi_close_device(retinst->spi);
		ufprog_spi_nor_destroy(retinst->snor);
		retinst->snor = NULL;
		return UFP_FAIL;
	}

	if (part)
		ret = ufprog_spi_nor_part_init(retinst->snor, NULL, part, false);
	else
		ret = ufprog_spi_nor_probe_init(retinst->snor);

	if (ret) {
		if (ret == UFP_FLASH_PART_NOT_RECOGNISED)
			os_fprintf(stderr, "Flash chip not recognised\n");
		else
			os_fprintf(stderr, "Flash probing failed\n");
		os_printf("\n");

		ufprog_spi_nor_detach(retinst->snor, true);
		ufprog_spi_nor_destroy(retinst->snor);
		retinst->snor = NULL;

		if (ret != UFP_FLASH_PART_NOT_RECOGNISED)
			return ret;

		goto errout;
	}

	ufprog_spi_nor_info(retinst->snor, &retinst->info);

	retinst->max_read_granularity = ufprog_spi_max_read_granularity(retinst->spi);
	retinst->speed = ufprog_spi_nor_get_speed_high(retinst->snor);

	ufprog_spi_get_speed_limit(retinst->spi, NULL, &retinst->max_speed);

	os_printf("Manufacturer:       %s\n", retinst->info.vendor);
	os_printf("Part:               %s\n", retinst->info.model);

	if (retinst->info.size < SZ_1K) {
		size = retinst->info.size;
		unit = "";
	} else if (retinst->info.size < SZ_1M) {
		size = retinst->info.size >> 10;
		unit = "K";
	} else {
		size = retinst->info.size >> 20;
		unit = "M";
	}

	if (retinst->info.ndies > 1)
		os_printf("Capacity:           %" PRIu64 "%sB * %u\n", size, unit, retinst->info.ndies);
	else
		os_printf("Capacity:           %" PRIu64 "%sB\n", size, unit);

	if (retinst->speed) {
		if (retinst->speed < 1000) {
			size = retinst->speed;
			unit = "";
		} else if (retinst->speed < 1000000) {
			size = retinst->speed / 1000;
			unit = "K";
		} else {
			size = retinst->speed / 1000000;
			unit = "M";
		}

		os_printf("Clock:              %" PRIu64 "%sHz\n", size, unit);
	}

	os_printf("\n");

	return UFP_OK;

errout:
	retinst->spi = NULL;
	retinst->snor = NULL;

	if (!allow_fail)
		return UFP_FAIL;

	return UFP_OK;
}

static void print_speed(uint64_t size, uint64_t time_us)
{
	const char *speed_unit;
	double speed;

	if (!time_us)
		time_us = 1;

	speed = (double)(size * 1000000) / (double)time_us;
	if (speed < 1024.0) {
		speed_unit = "";
	} else if (speed < 1048576.0) {
		speed_unit = "K";
		speed /= 1024.0;
	} else {
		speed_unit = "M";
		speed /= 1048576.0;
	}

	os_printf("Time used: %.2fs, speed: %.2f%sB/s\n", (double)time_us / 1000000.0, speed, speed_unit);
}

static ufprog_status read_flash_die(struct ufsnor_instance *inst, uint64_t addr, uint64_t size, void *buf,
				    uint64_t base_addr, uint64_t base_size, uint64_t total_size)
{
	uint32_t percentage, last_percentage = 0;
	size_t chksz, read_granularity;
	uint64_t sizerd = 0;
	ufprog_status ret;
	uint8_t *p = buf;

	read_granularity = inst->max_read_granularity;
	if (read_granularity > UFSNOR_READ_GRANULARITY)
		read_granularity = UFSNOR_READ_GRANULARITY;

	ret = ufprog_spi_nor_set_bus_width(inst->snor, spi_mem_io_info_cmd_bw(inst->info.read_io_info));
	if (ret) {
		os_fprintf(stderr, "Failed to set I/O bus width\n");
		return ret;
	}

	while (sizerd < size) {
		if (size - sizerd > read_granularity)
			chksz = read_granularity;
		else
			chksz = size - sizerd;

		ret = ufprog_spi_nor_read_no_check(inst->snor, addr, chksz, p);
		if (ret) {
			os_fprintf(stderr, "Failed to read flash at 0x%" PRIx64 "\n", base_addr + addr);
			goto cleanup;
		}

		addr += chksz;
		p += chksz;
		sizerd += chksz;

		percentage = (uint32_t)(((base_size + sizerd) * 100) / total_size);
		if (percentage > last_percentage) {
			last_percentage = percentage;
			progress_show(last_percentage);
		}
	}

	ret = UFP_OK;

cleanup:
	if (ufprog_spi_nor_set_bus_width(inst->snor, inst->info.cmd_bw))
		os_fprintf(stderr, "Failed to reset I/O bus width\n");

	return ret;
}

ufprog_status read_flash(struct ufsnor_instance *inst, uint64_t addr, uint64_t size, void *buf)
{
	uint64_t dieaddr = 0, opaddr, opsize, sizerd = 0, total_size = size, t0, t1;
	ufprog_status ret = UFP_OK;
	uint8_t *p = buf;
	uint32_t die;

	os_printf("Reading from flash at 0x%" PRIx64 ", size 0x%" PRIx64 " ...\n", addr, size);

	progress_init();

	t0 = os_get_timer_us();

	for (die = inst->die_start; size && die < inst->die_start + inst->die_count; die++) {
		if (addr < dieaddr || addr >= dieaddr + inst->info.size)
			goto next;

		opaddr = addr - dieaddr;
		opsize = inst->info.size - opaddr;
		if (opsize > size)
			opsize = size;

		ret = ufprog_spi_nor_select_die(inst->snor, die);
		if (ret) {
			os_fprintf(stderr, "Failed to select Die %u\n", die);
			goto out;
		}

		ret = read_flash_die(inst, opaddr, opsize, p, dieaddr + opaddr, sizerd, total_size);
		if (ret) {
			os_fprintf(stderr, "Read failed on Die %u, addr 0x%" PRIx64 "\n", die, opaddr);
			goto out;
		}

		p += opsize;
		size -= opsize;
		addr += opsize;
		sizerd += opsize;

	next:
		dieaddr += inst->info.size;
	}

out:
	if (!ret) {
		t1 = os_get_timer_us();

		progress_done();
		print_speed(total_size, t1 - t0);
		os_printf("Succeeded\n");
	}

	return ret;
}

static ufprog_status dump_flash_die(struct ufsnor_instance *inst, uint64_t addr, uint64_t size, uint8_t *buf,
				    uint64_t base_addr)
{
	uint64_t sizerd = 0;
	ufprog_status ret;
	size_t chksz;

	ret = ufprog_spi_nor_set_bus_width(inst->snor, spi_mem_io_info_cmd_bw(inst->info.read_io_info));
	if (ret) {
		os_fprintf(stderr, "Failed to set I/O bus width\n");
		goto cleanup;
	}

	while (sizerd < size) {
		if (size - sizerd > UFSNOR_READ_GRANULARITY) {
			chksz = UFSNOR_READ_GRANULARITY;
			chksz -= addr % UFSNOR_READ_GRANULARITY;
		} else {
			chksz = size - sizerd;
		}

		ret = ufprog_spi_nor_read_no_check(inst->snor, addr, chksz, buf);
		if (ret) {
			os_fprintf(stderr, "Failed to read flash at 0x%" PRIx64 "\n", base_addr + addr);
			goto cleanup;
		}

		hexdump(buf, chksz, base_addr + addr, true);

		addr += chksz;
		sizerd += chksz;
	}

	ret = UFP_OK;

cleanup:
	ret = ufprog_spi_nor_set_bus_width(inst->snor, inst->info.cmd_bw);
	if (ret)
		os_fprintf(stderr, "Failed to reset I/O bus width\n");

	return ret;
}

ufprog_status dump_flash(struct ufsnor_instance *inst, uint64_t addr, uint64_t size)
{
	uint64_t dieaddr = 0, opaddr, opsize;
	ufprog_status ret = UFP_OK;
	uint8_t *buf;
	uint32_t die;

	buf = malloc(UFSNOR_READ_GRANULARITY);
	if (!buf) {
		os_fprintf(stderr, "No memory for flash data dump buffer\n");
		return UFP_NOMEM;
	}

	os_printf("Dump flash data at 0x%" PRIx64 ", size 0x%" PRIx64 ":\n", addr, size);

	for (die = inst->die_start; size && die < inst->die_start + inst->die_count; die++) {
		if (addr < dieaddr || addr >= dieaddr + inst->info.size)
			goto next;

		opaddr = addr - dieaddr;
		opsize = inst->info.size - opaddr;
		if (opsize > size)
			opsize = size;

		ret = ufprog_spi_nor_select_die(inst->snor, die);
		if (ret) {
			os_fprintf(stderr, "Failed to select Die %u\n", die);
			goto out;
		}

		ret = dump_flash_die(inst, opaddr, opsize, buf, dieaddr + opaddr);
		if (ret) {
			os_fprintf(stderr, "Read failed on Die %u, addr 0x%" PRIx64 "\n", die, opaddr);
			goto out;
		}

		size -= opsize;
		addr += opsize;

	next:
		dieaddr += inst->info.size;
	}

out:
	free(buf);

	return ret;
}

static ufprog_status verify_flash_die(struct ufsnor_instance *inst, uint64_t addr, uint64_t size, const void *buf,
				      uint64_t base_addr, uint64_t base_size, uint64_t total_size)
{
	uint32_t percentage, last_percentage = 0;
	const uint8_t *p = buf;
	size_t chksz, cmppos;
	uint64_t sizerd = 0;
	ufprog_status ret;

	if (!verify_buffer) {
		verify_buffer = malloc(UFSNOR_READ_GRANULARITY);
		if (!verify_buffer) {
			os_fprintf(stderr, "No memory for verify buffer\n");
			return UFP_NOMEM;
		}
	}

	ret = ufprog_spi_nor_set_bus_width(inst->snor, spi_mem_io_info_cmd_bw(inst->info.read_io_info));
	if (ret) {
		os_fprintf(stderr, "Failed to set I/O bus width\n");
		return ret;
	}

	while (sizerd < size) {
		if (size - sizerd > UFSNOR_READ_GRANULARITY)
			chksz = UFSNOR_READ_GRANULARITY;
		else
			chksz = size - sizerd;

		ret = ufprog_spi_nor_read_no_check(inst->snor, addr, chksz, verify_buffer);
		if (ret) {
			os_fprintf(stderr, "Failed to read flash at 0x%" PRIx64 "\n", base_addr + addr);
			goto cleanup;
		}

		if (!bufdiff(p, verify_buffer, chksz, &cmppos)) {
			os_fprintf(stderr, "Data at 0x%" PRIx64 " are different: expect 0x%02x, got 0x%02x\n",
				   sizerd + cmppos, p[cmppos], verify_buffer[cmppos]);
			ret = UFP_DATA_VERIFICATION_FAIL;
			goto cleanup;
		}

		addr += chksz;
		p += chksz;
		sizerd += chksz;

		percentage = (uint32_t)(((base_size + sizerd) * 100) / total_size);
		if (percentage > last_percentage) {
			last_percentage = percentage;
			progress_show(last_percentage);
		}
	}

	ret = UFP_OK;

cleanup:
	if (ufprog_spi_nor_set_bus_width(inst->snor, inst->info.cmd_bw))
		os_fprintf(stderr, "Failed to reset I/O bus width\n");

	return ret;
}

ufprog_status verify_flash(struct ufsnor_instance *inst, uint64_t addr, uint64_t size, const void *buf)
{
	uint64_t dieaddr = 0, opaddr, opsize, sizerd = 0, total_size = size, t0, t1;
	ufprog_status ret = UFP_OK;
	const uint8_t *p = buf;
	uint32_t die;

	os_printf("Verifying flash data at 0x%" PRIx64 ", size 0x%" PRIx64 " ...\n", addr, size);

	progress_init();

	t0 = os_get_timer_us();

	for (die = inst->die_start; size && die < inst->die_start + inst->die_count; die++) {
		if (addr < dieaddr || addr >= dieaddr + inst->info.size)
			goto next;

		opaddr = addr - dieaddr;
		opsize = inst->info.size - opaddr;
		if (opsize > size)
			opsize = size;

		ret = ufprog_spi_nor_select_die(inst->snor, die);
		if (ret) {
			os_fprintf(stderr, "Failed to select Die %u\n", die);
			goto out;
		}

		ret = verify_flash_die(inst, opaddr, opsize, p, dieaddr + opaddr, sizerd, total_size);
		if (ret) {
			os_fprintf(stderr, "Verification failed on Die %u, addr 0x%" PRIx64 "\n", die, opaddr);
			goto out;
		}

		p += opsize;
		size -= opsize;
		addr += opsize;
		sizerd += opsize;

	next:
		dieaddr += inst->info.size;
	}

out:
	if (!ret) {
		t1 = os_get_timer_us();

		progress_done();
		print_speed(total_size, t1 - t0);
		os_printf("Succeeded\n");
	}

	return ret;
}

static ufprog_status erase_flash_die(struct ufsnor_instance *inst, uint64_t addr, uint64_t size, uint64_t base_addr,
				     uint64_t base_size, uint64_t total_size)
{
	uint32_t len, percentage, last_percentage = 0;
	uint64_t end = addr + size, sizeerased = 0;
	ufprog_status ret;

	while (addr < end) {
		ret = ufprog_spi_nor_erase_at(inst->snor, addr, end - addr, &len);
		if (ret) {
			os_fprintf(stderr, "Failed to erase flash at 0x%" PRIx64 "\n", base_addr + addr);
			return ret;
		}

		if (!len) {
			logm_err("Erase not complete. 0x" PRIx64 " remained\n", end - addr);
			return UFP_FAIL;
		}

		addr += len;
		sizeerased += len;

		percentage = (uint32_t)(((base_size + sizeerased) * 100) / total_size);
		if (percentage > last_percentage) {
			last_percentage = percentage;
			progress_show(last_percentage);
		}
	}

	return UFP_OK;
}

ufprog_status erase_flash(struct ufsnor_instance *inst, uint64_t addr, uint64_t size)
{
	uint64_t end, dieaddr = 0, opaddr, opsize, sizeerased = 0, total_size, t0, t1;
	ufprog_status ret = UFP_OK;
	uint32_t die;

	ret = ufprog_spi_nor_get_erase_range(inst->snor, addr, size, &addr, &end);
	if (ret) {
		os_fprintf(stderr, "Failed to calculate erase region\n");
		return ret;
	}

	size = end - addr;
	total_size = size;

	os_printf("Erasing flash at 0x%" PRIx64 ", size 0x%" PRIx64 " ...\n", addr, size);

	progress_init();

	t0 = os_get_timer_us();

	for (die = inst->die_start; size && die < inst->die_start + inst->die_count; die++) {
		if (addr < dieaddr || addr >= dieaddr + inst->info.size)
			goto next;

		opaddr = addr - dieaddr;
		opsize = inst->info.size - opaddr;
		if (opsize > size)
			opsize = size;

		ret = ufprog_spi_nor_select_die(inst->snor, die);
		if (ret) {
			os_fprintf(stderr, "Failed to select Die %u\n", die);
			goto out;
		}

		ret = erase_flash_die(inst, opaddr, opsize, dieaddr + opaddr, sizeerased, total_size);
		if (ret) {
			os_fprintf(stderr, "Verification failed on Die %u, addr 0x%" PRIx64 "\n", die, opaddr);
			goto out;
		}

		size -= opsize;
		addr += opsize;
		sizeerased += opsize;

	next:
		dieaddr += inst->info.size;
	}

out:
	if (!ret) {
		t1 = os_get_timer_us();

		progress_done();
		print_speed(total_size, t1 - t0);
		os_printf("Succeeded\n");
	}

	return ret;
}

static ufprog_status write_flash_die_no_erase(struct ufsnor_instance *inst, uint64_t addr, size_t size, const void *buf,
					      uint64_t base_addr, uint64_t base_size, uint64_t total_size)
{
	uint32_t percentage, last_percentage = 0;
	size_t len, retlen, sizewr = 0;
	uint64_t wraddr = addr;
	const uint8_t *p = buf;
	ufprog_status ret;

	ret = ufprog_spi_nor_set_bus_width(inst->snor, spi_mem_io_info_cmd_bw(inst->info.pp_io_info));
	if (ret) {
		os_fprintf(stderr, "Failed to set I/O bus width\n");
		return ret;
	}

	while (sizewr < size) {
		len = size - sizewr;
		if (len > UFSNOR_WRITE_GRANULARITY)
			len = UFSNOR_WRITE_GRANULARITY;

		ret = ufprog_spi_nor_write_page_no_check(inst->snor, wraddr, len, p, &retlen);
		if (ret) {
			os_fprintf(stderr, "Failed to write flash at 0x%" PRIx64 "\n", base_addr + wraddr);
			goto cleanup;
		}

		wraddr += retlen;
		p += retlen;
		sizewr += retlen;

		percentage = (uint32_t)(((base_size + sizewr) * 100) / total_size);
		if (percentage > last_percentage) {
			last_percentage = percentage;
			progress_show(last_percentage);
		}
	}

	ret = UFP_OK;

cleanup:
	if (ufprog_spi_nor_set_bus_width(inst->snor, inst->info.cmd_bw))
		os_fprintf(stderr, "Failed to reset I/O bus width\n");

	return ret;
}

ufprog_status write_flash_no_erase(struct ufsnor_instance *inst, uint64_t addr, uint64_t size, const void *buf,
				   bool verify)
{
	uint64_t dieaddr = 0, opaddr, opsize, sizewr = 0, orig_addr = addr, total_size = size, t0, t1;
	ufprog_status ret = UFP_OK;
	const uint8_t *p = buf;
	uint32_t die;

	os_printf("Writing to flash at 0x%" PRIx64 ", size 0x%" PRIx64 " ...\n", addr, size);

	progress_init();

	t0 = os_get_timer_us();

	for (die = inst->die_start; size && die < inst->die_start + inst->die_count; die++) {
		if (addr < dieaddr || addr >= dieaddr + inst->info.size)
			goto next;

		opaddr = addr - dieaddr;
		opsize = inst->info.size - opaddr;
		if (opsize > size)
			opsize = size;

		ret = ufprog_spi_nor_select_die(inst->snor, die);
		if (ret) {
			os_fprintf(stderr, "Failed to select Die %u\n", die);
			goto out;
		}

		ret = write_flash_die_no_erase(inst, opaddr, opsize, p, dieaddr + opaddr, sizewr, total_size);
		if (ret) {
			os_fprintf(stderr, "Write failed on Die %u, addr 0x%" PRIx64 "\n", die, opaddr);
			goto out;
		}

		p += opsize;
		size -= opsize;
		addr += opsize;
		sizewr += opsize;

	next:
		dieaddr += inst->info.size;
	}

out:
	if (!ret) {
		t1 = os_get_timer_us();

		progress_done();
		print_speed(total_size, t1 - t0);
		os_printf("Succeeded\n");

		if (verify) {
			os_printf("\n");
			return verify_flash(inst, orig_addr, total_size, buf);
		}
	}

	return ret;
}

ufprog_status write_flash(struct ufsnor_instance *inst, uint64_t addr, size_t size, const void *buf, bool update,
			  bool verify)
{
	uint64_t erase_start, erase_end, backup_size;
	struct snor_update_backup_info backup_info[2];
	size_t backup_count = 0;
	ufprog_status ret;
	uint32_t i;

	ret = ufprog_spi_nor_get_erase_range(inst->snor, addr, size, &erase_start, &erase_end);
	if (ret) {
		os_fprintf(stderr, "Failed to calculate erase region\n");
		return ret;
	}

	if (update) {
		if (erase_start < addr) {
			backup_info[backup_count].addr = erase_start;
			backup_info[backup_count].size = addr - erase_start;
			backup_count++;
		}

		if (addr + size < erase_end) {
			backup_info[backup_count].addr = addr + size;
			backup_info[backup_count].size = erase_end - (addr + size);
			backup_count++;
		}

		if (backup_count) {
			backup_size = backup_info[0].size;
			if (backup_count > 1)
				backup_size += backup_info[1].size;

			backup_info[0].data = malloc(backup_size);
			if (!backup_info[0].data) {
				os_fprintf(stderr, "No memory for update backup data\n");
				return UFP_NOMEM;
			}

			if (backup_count > 1)
				backup_info[1].data = backup_info[0].data + backup_info[0].size;
		}

		for (i = 0; i < backup_count; i++) {
			ret = read_flash(inst, backup_info[i].addr, backup_info[i].size, backup_info[i].data);
			if (ret) {
				os_fprintf(stderr, "Failed to backup data\n");
				goto cleanup;
			}

			os_printf("\n");
		}
	}

	ret = erase_flash(inst, addr, size);
	if (ret)
		goto cleanup;

	os_printf("\n");

	ret = write_flash_no_erase(inst, addr, size, buf, verify);
	if (ret)
		goto cleanup;

	os_printf("\n");

	if (update) {
		for (i = 0; i < backup_count; i++) {
			ret = write_flash_no_erase(inst, backup_info[i].addr, backup_info[i].size, backup_info[i].data,
						   verify);
			if (ret) {
				os_fprintf(stderr, "Failed to restore data\n");
				goto cleanup;
			}

			os_printf("\n");
		}
	}

	ret = UFP_OK;

cleanup:
	if (update) {
		if (backup_count)
			free(backup_info[0].data);
	}

	return ret;
}
