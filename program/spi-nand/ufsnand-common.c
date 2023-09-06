// SPDX-License-Identifier: GPL-2.0-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash programmer common part
 */

#define _GNU_SOURCE
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
#include "ufsnand-common.h"

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

ufprog_status load_config(struct ufsnand_options *retcfg, const char *curr_device)
{
	struct json_object *jroot, *device_cfgs, *device_cfg;
	const char *last_device;
	ufprog_status ret;

	ret = json_open_config(os_prog_name(), &jroot);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST) {
			retcfg->last_device = NULL;
			retcfg->log_level = DEFAULT_LOG_LEVEL;
			retcfg->global_max_speed = UFSNAND_MAX_SPEED;
			retcfg->max_speed = UFSNAND_MAX_SPEED;
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

	ret = json_read_uint32(jroot, "max-speed-hz", &retcfg->global_max_speed, UFSNAND_MAX_SPEED);
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

ufprog_status save_config(const struct ufsnand_options *cfg)
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
			  struct ufsnand_instance *retinst, bool list_only)
{
	ufprog_bool nor_read_enabled = false;
	ufprog_status ret;
	uint64_t size;
	char *unit;

	retinst->spi = NULL;
	retinst->snand = NULL;

	if (!device_name && !list_only) {
		os_fprintf(stderr, "Device name not specified\n");
		return UFP_FAIL;
	}

	retinst->snand = ufprog_spi_nand_create();
	if (!retinst->snand) {
		os_fprintf(stderr, "Failed to create spi-nand instance\n");
		return UFP_NOMEM;
	}

	if (list_only)
		return UFP_OK;

	if (!max_speed)
		max_speed = UFSNAND_MAX_SPEED;

	ufprog_spi_nand_set_speed_limit(retinst->snand, max_speed);

	ret = ufprog_spi_open_device(device_name, false, &retinst->spi);
	if (ret) {
		os_fprintf(stderr, "Failed to open device '%s'\n", device_name);
		goto errout_spi;
	}

	os_printf("\n");

	ret = ufprog_spi_nand_attach(retinst->snand, retinst->spi);
	if (ret) {
		os_fprintf(stderr, "Failed to attach spi interface to spi-nand instance\n");
		goto errout_snand;
	}

	if (part)
		ret = ufprog_spi_nand_part_init(retinst->snand, NULL, part);
	else
		ret = ufprog_spi_nand_probe_init(retinst->snand);

	if (ret) {
		if (ret == UFP_FLASH_PART_NOT_RECOGNISED)
			os_fprintf(stderr, "Flash chip not recognised\n");
		else
			os_fprintf(stderr, "Flash probing failed\n");
		os_printf("\n");

		goto errout_snand_detach;
	}

	retinst->nand.chip = ufprog_spi_nand_get_generic_nand_interface(retinst->snand);

	ufprog_spi_nand_info(retinst->snand, &retinst->sinfo);
	ufprog_nand_info(retinst->nand.chip, &retinst->nand.info);

	retinst->speed = ufprog_spi_nand_get_speed_high(retinst->snand);

	ufprog_spi_get_speed_limit(retinst->spi, NULL, &retinst->max_speed);

	os_printf("Manufacturer:       %s\n", retinst->nand.info.vendor);
	os_printf("Part:               %s\n", retinst->nand.info.model);

	if (retinst->nand.info.maux.size < SZ_1K) {
		size = retinst->nand.info.maux.size;
		unit = "";
	} else if (retinst->nand.info.maux.size < SZ_1M) {
		size = retinst->nand.info.maux.size >> 10;
		unit = "K";
	} else if (retinst->nand.info.maux.size < SZ_1G) {
		size = retinst->nand.info.maux.size >> 20;
		unit = "M";
	} else {
		size = retinst->nand.info.maux.size >> 30;
		unit = "G";
	}

	os_printf("Capacity:           %" PRIu64 "%sB\n", size, unit);
	os_printf("Page size:          %uKB+%uB\n", retinst->nand.info.memorg.page_size >> 10,
		  retinst->nand.info.memorg.oob_size);

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

	ret = ufprog_spi_nand_nor_read_enabled(retinst->snand, &nor_read_enabled);
	if (nor_read_enabled) {
		os_printf("SPI-NOR read timing emulation is enabled.\n");
		os_printf("\n");
	}

	return UFP_OK;

errout_snand_detach:
	ufprog_spi_nand_detach(retinst->snand, false);

errout_snand:
	ufprog_spi_nand_destroy(retinst->snand);
	retinst->snand = NULL;

errout_spi:
	ufprog_spi_close_device(retinst->spi);
	retinst->spi = NULL;

	return ret;
}

static bool is_internal_plugin_config_name(const char *name)
{
	size_t len = strlen(name), jsonlen = strlen(UFPROG_CONFIG_SUFFIX);

	if (strchr(name, '/') || strchr(name, '\\'))
		return false;

	if (len < jsonlen)
		return true;

	if (!strcmp(name + len - jsonlen, UFPROG_CONFIG_SUFFIX))
		return false;

	return true;
}

ufprog_status open_ecc_chip(const char *ecc_cfg, uint32_t page_size, uint32_t spare_size,
			    struct ufprog_nand_ecc_chip **outecc)
{
	const char *comma, *ecc_plguin, *ecc_cfg_file, *plugin_name;
	struct json_object *jobj = NULL, *jcfg = NULL;
	ufprog_status ret;
	int rc;

	comma = strchr(ecc_cfg, ',');
	if (comma) {
		rc = asprintf((char **)&ecc_plguin, "%.*s", (uint32_t)(comma - ecc_cfg), ecc_cfg);
		if (rc < 0)
			return UFP_NOMEM;

		ecc_cfg_file = comma + 1;
	} else {
		ecc_plguin = ecc_cfg;
		ecc_cfg_file = NULL;
	}

	if (!*ecc_plguin || !strcmp(ecc_plguin, "none")) {
		*outecc = NULL;
		ret = UFP_OK;
		goto cleanup;
	}

	plugin_name = ecc_plguin;

	if (is_internal_plugin_config_name(ecc_plguin)) {
		ret = ufprog_load_ecc_config(ecc_plguin, &jobj);
		if (!ret) {
			ret = json_read_str(jobj, "driver", &plugin_name, NULL);
			if (ret) {
				os_printf("Missing ECC plugin name in %s.json\n", ecc_plguin);
				goto cleanup;
			}

			ret = json_read_obj(jobj, "config", &jcfg);
			if (ret) {
				os_printf("Invalid ECC config in %s.json\n", ecc_plguin);
				goto cleanup;
			}
		}
	}

	if (!jobj && ecc_cfg_file && *ecc_cfg_file) {
		ret = json_from_file(ecc_cfg_file, &jobj);
		if (ret) {
			os_printf("Failed to load config file for ECC plugin.\n");
			goto cleanup;
		}

		jcfg = jobj;
	}

	ret = ufprog_ecc_open_chip(plugin_name, ecc_cfg, page_size, spare_size, jcfg, outecc);
	if (ret) {
		os_printf("Failed to load ECC plugin '%s'.\n", ecc_plguin);
		goto cleanup;
	}

	os_printf("\n");

cleanup:
	if (jobj)
		json_free(jobj);

	if (comma)
		free((void *)ecc_plguin);

	return ret;
}

ufprog_status open_bbt(const char *bbt_cfg, struct nand_chip *nand, struct ufprog_nand_bbt **outbbt)
{
	const char *comma, *bbt_plguin, *bbt_cfg_file, *plugin_name;
	struct json_object *jobj = NULL, *jcfg = NULL;
	ufprog_status ret;
	int rc;

	if (!bbt_cfg || !*bbt_cfg) {
		ret = ufprog_bbt_ram_create("default-bbt", nand, outbbt);
		if (ret) {
			os_fprintf(stderr, "Failed to create default BBT\n");
			return ret;
		}

		return UFP_OK;
	}

	comma = strchr(bbt_cfg, ',');
	if (comma) {
		rc = asprintf((char **)&bbt_plguin, "%.*s", (uint32_t)(comma - bbt_cfg), bbt_cfg);
		if (rc < 0)
			return UFP_NOMEM;

		bbt_cfg_file = comma + 1;
	} else {
		bbt_plguin = bbt_cfg;
		bbt_cfg_file = NULL;
	}

	plugin_name = bbt_plguin;

	if (is_internal_plugin_config_name(bbt_plguin)) {
		ret = ufprog_load_bbt_config(bbt_plguin, &jobj);
		if (!ret) {
			ret = json_read_str(jobj, "driver", &plugin_name, NULL);
			if (ret) {
				os_printf("Missing BBT plugin name in %s.json\n", bbt_plguin);
				goto cleanup;
			}

			ret = json_read_obj(jobj, "config", &jcfg);
			if (ret) {
				os_printf("Invalid BBT config in %s.json\n", bbt_plguin);
				goto cleanup;
			}
		}
	}

	if (!jobj && bbt_cfg_file && *bbt_cfg_file) {
		ret = json_from_file(bbt_cfg_file, &jobj);
		if (ret) {
			os_printf("Failed to load config file for BBT plugin.\n");
			goto cleanup;
		}

		jcfg = jobj;
	}

	ret = ufprog_bbt_create(plugin_name, bbt_cfg, nand, jcfg, outbbt);
	if (ret) {
		os_printf("Failed to load BBT plugin '%s'.\n", bbt_plguin);
		goto cleanup;
	}

	os_printf("\n");

cleanup:
	if (jobj)
		json_free(jobj);

	if (comma)
		free((void *)bbt_plguin);

	return ret;
}

ufprog_status open_ftl(const char *ftl_cfg, struct nand_chip *nand, struct ufprog_nand_bbt *bbt,
		       struct ufprog_nand_ftl **outftl, bool *ret_bbt_used)
{
	const char *comma = NULL, *ftl_plguin, *ftl_cfg_file, *plugin_name;
	struct json_object *jobj = NULL, *jcfg = NULL;
	uint32_t default_ftl_flags = 0;
	ufprog_status ret;
	int rc;

	if (!ftl_cfg || !*ftl_cfg) {
		ftl_plguin = "default-ftl";
		goto create_default;
	}

	comma = strchr(ftl_cfg, ',');
	if (comma) {
		rc = asprintf((char **)&ftl_plguin, "%.*s", (uint32_t)(comma - ftl_cfg), ftl_cfg);
		if (rc < 0)
			return UFP_NOMEM;

		ftl_cfg_file = comma + 1;
	} else {
		ftl_plguin = ftl_cfg;
		ftl_cfg_file = NULL;
	}

	if (!*ftl_plguin || !strcmp(ftl_plguin, "none")) {
		default_ftl_flags = FTL_BASIC_F_DONT_CHECK_BAD;

	create_default:
		ret = ufprog_ftl_basic_create(ftl_plguin, nand, bbt, default_ftl_flags, outftl);
		if (ret)
			os_fprintf(stderr, "Failed to create default FTL\n");

		*ret_bbt_used = true;

		goto cleanup_plugin_name;
	}

	plugin_name = ftl_plguin;

	if (is_internal_plugin_config_name(ftl_plguin)) {
		ret = ufprog_load_ftl_config(ftl_plguin, &jobj);
		if (!ret) {
			ret = json_read_str(jobj, "driver", &plugin_name, NULL);
			if (ret) {
				os_printf("Missing FTL plugin name in %s.json\n", ftl_plguin);
				goto cleanup;
			}

			ret = json_read_obj(jobj, "config", &jcfg);
			if (ret) {
				os_printf("Invalid FTL config in %s.json\n", ftl_plguin);
				goto cleanup;
			}
		}
	}

	if (!jobj && ftl_cfg_file && *ftl_cfg_file) {
		ret = json_from_file(ftl_cfg_file, &jobj);
		if (ret) {
			os_printf("Failed to load config file for FTL plugin.\n");
			goto cleanup;
		}

		jcfg = jobj;
	}

	ret = ufprog_ftl_create(plugin_name, ftl_cfg, nand, jcfg, outftl);
	if (ret) {
		os_printf("Failed to load FTL plugin '%s'.\n", ftl_plguin);
		goto cleanup;
	}

	*ret_bbt_used = false;

	os_printf("\n");

cleanup:
	if (jobj)
		json_free(jobj);

cleanup_plugin_name:
	if (comma)
		free((void *)ftl_plguin);

	return ret;
}

uint32_t print_bbt(struct ufnand_instance *nandinst, struct ufprog_nand_bbt *bbt)
{
	uint32_t i, cnt = 0;
	uint64_t addr = 0;

	for (i = 0; i < nandinst->info.maux.block_count; i++, addr += nandinst->info.maux.block_size) {
		if (ufprog_bbt_is_bad(bbt, i)) {
			os_printf("Bad block %u at 0x%" PRIx64 "\n", i, addr);
			cnt++;
		}
	}

	return cnt;
}

void print_speed(uint64_t size, uint64_t time_us)
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

static void nand_progressbar_cb(struct ufnand_progress_status *prog, uint32_t count)
{
	uint32_t percentage;

	prog->current += count;
	percentage = prog->current * 100ULL / prog->total;

	if (percentage > prog->last_percentage)
		progress_show(percentage);

	prog->last_percentage = percentage;
}

static void nand_progressbar_init(struct ufnand_progress_status *prog, uint32_t total_count)
{
	prog->total = total_count;
	prog->current = 0;
	prog->last_percentage = 0;

	progress_init();
}

static void nand_progressbar_done(struct ufnand_progress_status *prog)
{
	progress_done();
}

static void print_rwe_status(struct ufnand_rwe_data *rwedata, bool read, bool dump)
{
	const char *oob_state, *ecc_state, *fmt_state = NULL;

	if (rwedata->oob)
		oob_state = "included";
	else
		oob_state = "excluded";

	if (rwedata->raw)
		ecc_state = "disabled";
	else
		ecc_state = "enabled";

	if (rwedata->fmt) {
		if (read)
			fmt_state = "Input";
		else
			fmt_state = "Output";
	}

	if (fmt_state) {
		if (dump)
			os_printf("OOB: %s. ECC: %s. Data is in canonical layout.\n", oob_state, ecc_state);
		else
			os_printf("OOB: %s. ECC: %s. %s file is in canonical layout.\n", oob_state, ecc_state,
				  fmt_state);
	} else {
		os_printf("OOB: %s. ECC: %s.\n", oob_state, ecc_state);
	}
}

static ufprog_status nand_process_read_page_data(struct ufnand_instance *nandinst, struct ufnand_op_data *opdata,
						 void *dst, const void *src, uint32_t count, bool fmt)
{
	const uint8_t *s = src;
	ufprog_status ret;
	uint8_t *p = dst;
	uint32_t i;

	for (i = 0; i < count; i++) {
		if (!fmt) {
			memcpy(p, s, opdata->page_size);
		} else {
			ret = ufprog_nand_convert_page_format(nandinst->chip, s, opdata->buf[1], false);
			if (ret) {
				os_fprintf(stderr, "Failed to convert page data\n");
				return ret;
			}

			memcpy(p, opdata->buf[1], opdata->page_size);
		}

		p += opdata->page_size;
		s += nandinst->info.maux.oob_page_size;
	}

	return UFP_OK;
}

static ufprog_status nand_ftl_read_post_cb(struct ufprog_ftl_callback *cb, uint32_t actual_count)
{
	struct ufnand_ftl_callback *ftlcb = container_of(cb, struct ufnand_ftl_callback, cb);
	ufprog_status ret;

	ret = nand_process_read_page_data(ftlcb->nandinst, ftlcb->opdata, ftlcb->buf.rx, ftlcb->cb.buffer,
					  actual_count, ftlcb->rwedata->fmt);
	if (ret)
		return ret;

	ftlcb->buf.rx += ftlcb->opdata->page_size * actual_count;

	nand_progressbar_cb(&ftlcb->prog, actual_count);

	return UFP_OK;
}

ufprog_status nand_read(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata,
			const struct ufprog_ftl_part *part, struct ufnand_op_data *opdata, file_mapping fm,
			uint32_t page, uint32_t count)
{
	uint64_t total_size, map_offset, real_map_offset, t0, t1;
	uint32_t real_page, num_to_read, retnum;
	struct ufnand_ftl_callback ftlcb;
	ufprog_status ret = UFP_OK;
	uint8_t *map_base;
	size_t map_size;

	total_size = (uint64_t)opdata->page_size * count;

	if (part->base_block && page) {
		os_printf("Reading from flash at relative page %u (0x%" PRIx64 "), count %u (size 0x%" PRIx64 ") ...\n",
			  page, (uint64_t)page << nandinst->info.maux.page_shift, count, total_size);
	} else {
		real_page = (part->base_block << nandinst->info.maux.pages_per_block_shift) + page;
		os_printf("Reading from flash at page %u (0x%" PRIx64 "), count %u (size 0x%" PRIx64 ") ...\n",
			  real_page, (uint64_t)real_page << nandinst->info.maux.page_shift, count, total_size);
	}

	print_rwe_status(rwedata, true, false);

	if (!os_set_file_mapping_offset(fm, 0, (void **)&map_base)) {
		os_fprintf(stderr, "Failed to adjust file mapping\n");
		return UFP_FAIL;
	}

	map_offset = os_get_file_mapping_offset(fm);
	map_size = os_get_file_mapping_size(fm);

	memset(&ftlcb, 0, sizeof(ftlcb));
	nand_progressbar_init(&ftlcb.prog, count);
	ftlcb.cb.post = nand_ftl_read_post_cb;
	ftlcb.cb.buffer = opdata->buf[0];
	ftlcb.nandinst = nandinst;
	ftlcb.rwedata = rwedata;
	ftlcb.opdata = opdata;

	t0 = os_get_timer_us();

	while (count) {
		if (!rwedata->oob)
			num_to_read = (uint32_t)(map_size >> nandinst->info.maux.page_shift);
		else
			num_to_read = (uint32_t)(map_size / nandinst->info.maux.oob_page_size);

		if (num_to_read > count)
			num_to_read = count;

		ftlcb.buf.rx = map_base;

		ret = ufprog_ftl_read_pages(nandinst->ftl, part, page, num_to_read, NULL, rwedata->raw,
					    NAND_READ_F_IGNORE_ECC_ERROR, &retnum, &ftlcb.cb);
		if (ret) {
			if (ret == UFP_FLASH_ADDRESS_OUT_OF_RANGE) {
				count -= retnum;
				page += retnum;
			}
			break;
		}

		count -= num_to_read;
		page += num_to_read;

		if (!count)
			break;

		/* Move file mapping */
		map_offset += (size_t)opdata->page_size * num_to_read;

		if (!os_set_file_mapping_offset(fm, map_offset, (void **)&map_base)) {
			os_fprintf(stderr, "Failed to adjust file mapping\n");
			ret = UFP_FAIL;
			break;
		}

		real_map_offset = os_get_file_mapping_offset(fm);
		map_size = os_get_file_mapping_size(fm);

		if (real_map_offset < map_offset) {
			map_size -= map_offset - real_map_offset;
			map_base += map_offset - real_map_offset;
		}
	}

	if (!ret) {
		t1 = os_get_timer_us();

		nand_progressbar_done(&ftlcb.prog);
		print_speed(total_size, t1 - t0);
		os_printf("Succeeded\n");
	}

	if (ret == UFP_FLASH_ADDRESS_OUT_OF_RANGE) {
		os_fprintf(stderr, "0x%" PRIx64 " remained to be read\n",
			   (uint64_t)count << nandinst->info.maux.page_shift);
	}

	return ret;
}

ufprog_status nand_dump(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata,
			const struct ufprog_ftl_part *part, struct ufnand_op_data *opdata, uint32_t page,
			uint32_t count)
{
	bool show_all = false, fmt = false;
	struct ufprog_nand_ecc_chip *ecc;
	ufprog_status ret = UFP_OK;
	uint32_t real_page = 0;
	const uint8_t *p;

	ecc = ufprog_nand_get_ecc(nandinst->chip);
	if (ecc) {
		if (ufprog_ecc_support_convert_page_layout(ecc)) {
			if (!rwedata->fmt)
				show_all = true;
			else
				fmt = true;
		}
	}

	if (part->base_block && page) {
		os_printf("Dump of flash at relative page %u (0x%" PRIx64 "), count %u ...\n",
			  page, (uint64_t)page << nandinst->info.maux.page_shift, count);
	} else {
		real_page = (part->base_block << nandinst->info.maux.pages_per_block_shift) + page;
		os_printf("Dump of flash at page %u (0x%" PRIx64 "), count %u ...\n",
			  real_page, (uint64_t)real_page << nandinst->info.maux.page_shift, count);
	}

	print_rwe_status(rwedata, true, true);

	while (count) {
		ret = ufprog_ftl_read_pages(nandinst->ftl, part, page, 1, opdata->buf[0], rwedata->raw,
					    NAND_READ_F_IGNORE_ECC_ERROR, NULL, NULL);
		if (ret)
			break;

		os_printf("\n");

		if (part->base_block && page) {
			os_printf("Dump data of page %u+%u (0x%" PRIx64 "+0x%" PRIx64 "):\n",
				  part->base_block << nandinst->info.maux.pages_per_block_shift, page,
				  (uint64_t)part->base_block << nandinst->info.maux.block_shift,
				  (uint64_t)page << nandinst->info.maux.page_shift);
		} else {
			os_printf("Dump data of page %u (0x%" PRIx64 "):\n",
				  real_page, (uint64_t)real_page << nandinst->info.maux.page_shift);
		}

		if (show_all) {
			hexdump(opdata->buf[0], nandinst->info.maux.oob_page_size,
				(uint64_t)page << nandinst->info.maux.page_shift, true);
		} else {
			if (fmt) {
				ret = ufprog_nand_convert_page_format(nandinst->chip, opdata->buf[0], opdata->buf[1],
								      false);
				if (ret) {
					os_fprintf(stderr, "Failed to convert page data\n");
					return ret;
				}

				p = opdata->buf[1];
			} else {
				p = opdata->buf[0];
			}

			hexdump(p, nandinst->info.memorg.page_size,
				(uint64_t)page << nandinst->info.maux.page_shift, true);

			if (rwedata->oob) {
				os_printf("\n");
				os_printf("OOB:\n");

				hexdump(p + nandinst->info.memorg.page_size, nandinst->info.memorg.oob_size, 0, true);
			}
		}

		count--;
		page++;
		real_page++;
	}

	if (ret == UFP_FLASH_ADDRESS_OUT_OF_RANGE) {
		os_fprintf(stderr, "%u page(s) remained to be dumped\n",
			   count);
	}

	return ret;
}

static ufprog_status nand_prepare_write_page_data(struct ufnand_instance *nandinst, struct ufnand_op_data *opdata,
						  void *dst, const void *src, uint32_t count, bool fmt)
{
	uint32_t i, flags;
	const uint8_t *s = src;
	ufprog_status ret;
	uint8_t *p = dst;

	flags = PAGE_FILL_F_FILL_NON_DATA_FF | PAGE_FILL_F_FILL_OOB | PAGE_FILL_F_FILL_UNPROTECTED_OOB |
		PAGE_FILL_F_SRC_SKIP_NON_DATA;

	for (i = 0; i < count; i++) {
		if (!fmt) {
			ufprog_nand_fill_page_by_layout(opdata->layout, p, s, opdata->page_size, flags);
		} else {
			ufprog_nand_fill_page_by_layout(opdata->layout, opdata->buf[1], s, opdata->page_size, flags);

			ret = ufprog_nand_convert_page_format(nandinst->chip, opdata->buf[1], p, true);
			if (ret) {
				os_fprintf(stderr, "Failed to convert page data\n");
				return ret;
			}
		}

		p += nandinst->info.maux.oob_page_size;
		s += opdata->page_size;
	}

	return UFP_OK;
}

static ufprog_status nand_ftl_write_pre_cb(struct ufprog_ftl_callback *cb, uint32_t requested_count)
{
	struct ufnand_ftl_callback *ftlcb = container_of(cb, struct ufnand_ftl_callback, cb);
	ufprog_status ret;
	const void *data;

	/* Process all but last page */
	ret = nand_prepare_write_page_data(ftlcb->nandinst, ftlcb->opdata, ftlcb->cb.buffer, ftlcb->buf.tx,
					   requested_count - 1, ftlcb->rwedata->fmt);
	if (ret)
		return ret;

	/* Process last page */
	if (ftlcb->last_batch && ftlcb->last_page_padding &&
	    ftlcb->count_left <= ftlcb->nandinst->info.memorg.pages_per_block) {
		memcpy(ftlcb->opdata->tmp, ftlcb->buf.tx + ftlcb->opdata->page_size * (requested_count - 1),
		       ftlcb->opdata->page_size - ftlcb->last_page_padding);
		memset(ftlcb->opdata->tmp + ftlcb->opdata->page_size - ftlcb->last_page_padding, 0xff,
		       ftlcb->last_page_padding);

		data = ftlcb->opdata->tmp;
	} else {
		data = ftlcb->buf.tx + ftlcb->opdata->page_size * (requested_count - 1);
	}

	ret = nand_prepare_write_page_data(ftlcb->nandinst, ftlcb->opdata,
					   (uint8_t *)ftlcb->cb.buffer +
					   ftlcb->nandinst->info.maux.oob_page_size * (requested_count - 1),
					   data, 1, ftlcb->rwedata->fmt);
	if (ret)
		return ret;

	ftlcb->buf.tx += ftlcb->opdata->page_size * requested_count;
	ftlcb->count_left -= requested_count;

	return UFP_OK;
}

static ufprog_status nand_ftl_write_post_cb(struct ufprog_ftl_callback *cb, uint32_t actual_count)
{
	struct ufnand_ftl_callback *ftlcb = container_of(cb, struct ufnand_ftl_callback, cb);

	nand_progressbar_cb(&ftlcb->prog, actual_count);

	return UFP_OK;
}

ufprog_status nand_write(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata,
			 const struct ufprog_ftl_part *part, struct ufnand_op_data *opdata, file_mapping fm,
			 uint32_t page, uint32_t count, uint32_t last_page_padding)
{
	uint64_t total_size, map_offset, real_map_offset, t0, t1;
	uint32_t real_page, num_to_write, retnum;
	struct ufnand_ftl_callback ftlcb;
	ufprog_status ret = UFP_OK;
	uint8_t *map_base;
	size_t map_size;

	total_size = (uint64_t)opdata->page_size * count;

	if (part->base_block && page) {
		os_printf("Writing to flash at relative page %u (0x%" PRIx64 "), count %u (size 0x%" PRIx64 ") ...\n",
			  page, (uint64_t)page << nandinst->info.maux.page_shift, count, total_size);
	} else {
		real_page = (part->base_block << nandinst->info.maux.pages_per_block_shift) + page;
		os_printf("Writing to flash at page %u (0x%" PRIx64 "), count %u (size 0x%" PRIx64 ") ...\n",
			  real_page, (uint64_t)real_page << nandinst->info.maux.page_shift, count, total_size);
	}

	print_rwe_status(rwedata, true, false);

	if (!os_set_file_mapping_offset(fm, 0, (void **)&map_base)) {
		os_fprintf(stderr, "Failed to adjust file mapping\n");
		return UFP_FAIL;
	}

	map_offset = os_get_file_mapping_offset(fm);
	map_size = os_get_file_mapping_size(fm);

	memset(&ftlcb, 0, sizeof(ftlcb));
	nand_progressbar_init(&ftlcb.prog, count);
	ftlcb.cb.pre = nand_ftl_write_pre_cb;
	ftlcb.cb.post = nand_ftl_write_post_cb;
	ftlcb.cb.buffer = opdata->buf[0];
	ftlcb.nandinst = nandinst;
	ftlcb.rwedata = rwedata;
	ftlcb.opdata = opdata;
	ftlcb.last_page_padding = last_page_padding;
	ftlcb.count_left = count;

	t0 = os_get_timer_us();

	while (count) {
		if (!rwedata->oob)
			num_to_write = (uint32_t)(map_size >> nandinst->info.maux.page_shift);
		else
			num_to_write = (uint32_t)(map_size / nandinst->info.maux.oob_page_size);

		if (!num_to_write || num_to_write > count)
			num_to_write = count;

		ftlcb.last_batch = num_to_write == count;
		ftlcb.buf.tx = map_base;

		ret = ufprog_ftl_write_pages(nandinst->ftl, part, page, num_to_write, NULL, rwedata->raw,
					     !rwedata->nospread, &retnum, &ftlcb.cb);
		if (ret) {
			if (ret == UFP_FLASH_ADDRESS_OUT_OF_RANGE) {
				count -= retnum;
				page += retnum;
			}
			break;
		}

		count -= num_to_write;
		page += num_to_write;

		if (!count)
			break;

		/* Move file mapping */
		map_offset += (size_t)opdata->page_size * num_to_write;

		if (!os_set_file_mapping_offset(fm, map_offset, (void **)&map_base)) {
			os_fprintf(stderr, "Failed to adjust file mapping\n");
			ret = UFP_FAIL;
			break;
		}

		real_map_offset = os_get_file_mapping_offset(fm);
		map_size = os_get_file_mapping_size(fm);

		if (real_map_offset < map_offset) {
			map_size -= map_offset - real_map_offset;
			map_base += map_offset - real_map_offset;
		}
	}

	if (!ret) {
		t1 = os_get_timer_us();

		nand_progressbar_done(&ftlcb.prog);
		print_speed(total_size, t1 - t0);
		os_printf("Succeeded\n");
	}

	if (ret == UFP_FLASH_ADDRESS_OUT_OF_RANGE) {
		os_fprintf(stderr, "0x%" PRIx64 " remained to be written\n",
			   (uint64_t)count << nandinst->info.maux.page_shift);
	}

	return ret;
}

static bool nand_verify_page(struct ufnand_op_data *opdata, const uint8_t *buf, const uint8_t *gold, uint32_t page,
			     uint32_t verify_len)
{
	bool check;
	uint32_t i;

	for (i = 0; i < verify_len; i++) {
		switch (opdata->map[i]) {
		case NAND_PAGE_BYTE_DATA:
		case NAND_PAGE_BYTE_OOB_DATA:
		case NAND_PAGE_BYTE_OOB_FREE:
			check = true;
			break;

		default:
			check = false;
		}

		if (check && buf[i] != gold[i]) {
			os_fprintf(stderr, "Page %u data at 0x%x are different: expect 0x%02x, got 0x%02x\n",
				   page, i, gold[i], buf[i]);
			return false;
		}
	}

	return true;
}

static ufprog_status nand_verify_buf(struct ufnand_instance *nandinst, struct ufnand_op_data *opdata,
				     const uint8_t *buf, const uint8_t *gold, uint32_t page, uint32_t count,
				     uint32_t verify_len, bool fmt)
{
	ufprog_status ret;
	const uint8_t *p;
	uint32_t i;
	bool rc;

	for (i = 0; i < count; i++) {
		if (fmt) {
			ret = ufprog_nand_convert_page_format(nandinst->chip, buf, opdata->buf[1], false);
			if (ret) {
				os_fprintf(stderr, "Failed to convert page data\n");
				return ret;
			}

			p = opdata->buf[1];
		} else {
			p = buf;
		}


		rc = nand_verify_page(opdata, p, gold, page, verify_len);
		if (!rc)
			return UFP_FAIL;

		gold += opdata->page_size;
		buf += nandinst->info.maux.oob_page_size;
	}

	return UFP_OK;
}

static ufprog_status nand_ftl_verify_post_cb(struct ufprog_ftl_callback *cb, uint32_t actual_count)
{
	struct ufnand_ftl_callback *ftlcb = container_of(cb, struct ufnand_ftl_callback, cb);
	uint32_t verify_len = ftlcb->opdata->page_size;
	ufprog_status ret;

	/* Verify all but last page */
	ret = nand_verify_buf(ftlcb->nandinst, ftlcb->opdata, ftlcb->cb.buffer, ftlcb->buf.rx, ftlcb->page,
			      actual_count - 1, verify_len, ftlcb->rwedata->fmt);
	if (ret)
		return ret;

	/* Verify last page */
	if (ftlcb->last_batch && ftlcb->last_page_padding &&
	    ftlcb->count_left <= ftlcb->nandinst->info.memorg.pages_per_block) {
		verify_len -= ftlcb->last_page_padding;
	}

	ret = nand_verify_buf(ftlcb->nandinst, ftlcb->opdata,
			      (const uint8_t *)ftlcb->cb.buffer +
			      ftlcb->nandinst->info.maux.oob_page_size * (actual_count - 1),
			      ftlcb->buf.rx + ftlcb->opdata->page_size * (actual_count - 1),
			      ftlcb->page + actual_count - 1, 1, verify_len, ftlcb->rwedata->fmt);
	if (ret)
		return ret;

	ftlcb->buf.rx += ftlcb->opdata->page_size * actual_count;
	ftlcb->count_left -= actual_count;

	nand_progressbar_cb(&ftlcb->prog, actual_count);

	return UFP_OK;
}

ufprog_status nand_verify(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata,
			  const struct ufprog_ftl_part *part, struct ufnand_op_data *opdata, file_mapping fm,
			  uint32_t page, uint32_t count, uint32_t last_page_padding)
{
	uint64_t total_size, map_offset, real_map_offset, t0, t1;
	uint32_t real_page, num_to_read, retnum;
	struct ufnand_ftl_callback ftlcb;
	ufprog_status ret = UFP_OK;
	uint8_t *map_base;
	size_t map_size;

	total_size = (uint64_t)opdata->page_size * count;

	if (part->base_block && page) {
		os_printf("Verifying flash at relative page %u (0x%" PRIx64 "), count %u (size 0x%" PRIx64 ") ...\n",
			  page, (uint64_t)page << nandinst->info.maux.page_shift, count, total_size);
	} else {
		real_page = (part->base_block << nandinst->info.maux.pages_per_block_shift) + page;
		os_printf("Verifying flash at page %u (0x%" PRIx64 "), count %u (size 0x%" PRIx64 ") ...\n",
			  real_page, (uint64_t)real_page << nandinst->info.maux.page_shift, count, total_size);
	}

	print_rwe_status(rwedata, true, false);

	if (!os_set_file_mapping_offset(fm, 0, (void **)&map_base)) {
		os_fprintf(stderr, "Failed to adjust file mapping\n");
		return UFP_FAIL;
	}

	map_offset = os_get_file_mapping_offset(fm);
	map_size = os_get_file_mapping_size(fm);

	memset(&ftlcb, 0, sizeof(ftlcb));
	nand_progressbar_init(&ftlcb.prog, count);
	ftlcb.cb.post = nand_ftl_verify_post_cb;
	ftlcb.cb.buffer = opdata->buf[0];
	ftlcb.nandinst = nandinst;
	ftlcb.rwedata = rwedata;
	ftlcb.opdata = opdata;
	ftlcb.last_page_padding = last_page_padding;
	ftlcb.count_left = count;

	t0 = os_get_timer_us();

	while (count) {
		if (!rwedata->oob)
			num_to_read = (uint32_t)(map_size >> nandinst->info.maux.page_shift);
		else
			num_to_read = (uint32_t)(map_size / nandinst->info.maux.oob_page_size);

		if (!num_to_read || num_to_read > count)
			num_to_read = count;

		ftlcb.last_batch = num_to_read == count;
		ftlcb.buf.rx = map_base;
		ftlcb.page = page;

		ret = ufprog_ftl_read_pages(nandinst->ftl, part, page, num_to_read, NULL, rwedata->raw,
					    NAND_READ_F_IGNORE_ECC_ERROR, &retnum, &ftlcb.cb);
		if (ret) {
			if (ret == UFP_FLASH_ADDRESS_OUT_OF_RANGE) {
				count -= retnum;
				page += retnum;
			}
			break;
		}

		count -= num_to_read;
		page += num_to_read;

		if (!count)
			break;

		/* Move file mapping */
		map_offset += (size_t)opdata->page_size * num_to_read;

		if (!os_set_file_mapping_offset(fm, map_offset, (void **)&map_base)) {
			os_fprintf(stderr, "Failed to adjust file mapping\n");
			ret = UFP_FAIL;
			break;
		}

		real_map_offset = os_get_file_mapping_offset(fm);
		map_size = os_get_file_mapping_size(fm);

		if (real_map_offset < map_offset) {
			map_size -= map_offset - real_map_offset;
			map_base += map_offset - real_map_offset;
		}
	}

	if (!ret) {
		t1 = os_get_timer_us();

		nand_progressbar_done(&ftlcb.prog);
		print_speed(total_size, t1 - t0);
		os_printf("Succeeded\n");
	}

	if (ret == UFP_FLASH_ADDRESS_OUT_OF_RANGE) {
		os_fprintf(stderr, "0x%" PRIx64 " remained to be verified\n",
			   (uint64_t)count << nandinst->info.maux.page_shift);
	}

	return ret;
}

static ufprog_status nand_ftl_erase_post_cb(struct ufprog_ftl_callback *cb, uint32_t actual_count)
{
	struct ufnand_ftl_callback *ftlcb = container_of(cb, struct ufnand_ftl_callback, cb);

	nand_progressbar_cb(&ftlcb->prog, actual_count);

	return UFP_OK;
}

ufprog_status nand_erase(struct ufnand_instance *nandinst, const struct ufprog_ftl_part *part, uint32_t page,
			 uint32_t count, bool nospread)
{
	uint32_t real_block, block, block_count, end, retcnt;
	struct ufnand_ftl_callback ftlcb;
	uint64_t total_size, t0, t1;
	ufprog_status ret;

	block = page >> nandinst->info.maux.pages_per_block_shift;
	end = (page + count + nandinst->info.memorg.pages_per_block - 1) >> nandinst->info.maux.pages_per_block_shift;
	block_count = end - block;

	total_size = (uint64_t)nandinst->info.maux.block_size * block_count;

	if (part->base_block && block) {
		os_printf("Erasing flash at relative block %u (0x%" PRIx64 "), count %u (size 0x%" PRIx64 ") ...\n",
			  block, (uint64_t)block << nandinst->info.maux.block_shift, block_count, total_size);
	} else {
		real_block = part->base_block + block;
		os_printf("Erasing flash at block %u (0x%" PRIx64 "), count %u (size 0x%" PRIx64 ") ...\n",
			  real_block, (uint64_t)real_block << nandinst->info.maux.block_shift, block_count, total_size);
	}

	memset(&ftlcb, 0, sizeof(ftlcb));
	nand_progressbar_init(&ftlcb.prog, block_count);
	ftlcb.cb.post = nand_ftl_erase_post_cb;

	t0 = os_get_timer_us();

	ret = ufprog_ftl_erase_blocks(nandinst->ftl, part, block, block_count, !nospread, &retcnt, &ftlcb.cb);

	if (!ret) {
		t1 = os_get_timer_us();

		nand_progressbar_done(&ftlcb.prog);
		print_speed(total_size, t1 - t0);
		os_printf("Succeeded\n");
	}

	if (ret == UFP_FLASH_ADDRESS_OUT_OF_RANGE) {
		os_fprintf(stderr, "%u block(s) (0x%" PRIx64 ") remained to be erased\n",
			   retcnt, (uint64_t)(block_count - retcnt) << nandinst->info.maux.block_shift);
	}

	return ret;
}
