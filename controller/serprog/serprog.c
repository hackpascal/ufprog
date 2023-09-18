/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Interface driver for SPI using serprog protocol
 */

#include <malloc.h>
#include <string.h>
#include <ufprog/api_controller.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include "serprog.h"

#define SERPROG_DRV_API_VER_MAJOR			1
#define SERPROG_DRV_API_VER_MINOR			0

ufprog_status UFPROG_API ufprog_plugin_init(void)
{
	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_plugin_cleanup(void)
{
	return UFP_OK;
}

const char *UFPROG_API ufprog_plugin_desc(void)
{
	return "serprog";
}

uint32_t UFPROG_API ufprog_controller_supported_if(void)
{
	return IFM_SPI;
}

static ufprog_status serprog_config_from_match(struct json_object *match, const char **retport,
					       struct serial_port_config *retconfig)
{
	uint32_t data_bits;
	const char *str;

	memset(retconfig, 0, sizeof(*retconfig));

	STATUS_CHECK_RET(json_read_str(match, "port", retport, NULL));

	if (!*retport) {
		logm_err("Serial port device not specified\n");
		return UFP_INVALID_PARAMETER;
	}

	STATUS_CHECK_RET(json_read_uint32(match, "baudrate", &retconfig->baudrate, SERPROG_DEFAULT_BAUDRATE));

	STATUS_CHECK_RET(json_read_uint32(match, "data-bits", &data_bits, SERPROG_DEFAULT_DATA_BITS));

	if (data_bits < 5 || data_bits > 8) {
		logm_err("Invalid data bits %u. Only 4-8 are valid\n", data_bits);
		return UFP_JSON_DATA_INVALID;
	}

	retconfig->data_bits = (uint8_t)data_bits;

	STATUS_CHECK_RET(json_read_str(match, "stop-bits", &str, "1"));

	if (!strcmp(str, "1")) {
		retconfig->stop_bits = SERIAL_STOP_BITS_1;
	} else if (!strcmp(str, "1.5")) {
		retconfig->stop_bits = SERIAL_STOP_BITS_1P5;
	} else if (!strcmp(str, "2")) {
		retconfig->stop_bits = SERIAL_STOP_BITS_2;
	} else {
		logm_err("Invalid stop bits %s. Only 1/1.5/2 are valid\n", str);
		return UFP_JSON_DATA_INVALID;
	}

	STATUS_CHECK_RET(json_read_str(match, "parity", &str, "none"));

	if (!strcmp(str, "none")) {
		retconfig->parity = SERIAL_PARITY_NONE;
	} else if (!strcmp(str, "odd")) {
		retconfig->parity = SERIAL_PARITY_ODD;
	} else if (!strcmp(str, "even")) {
		retconfig->parity = SERIAL_PARITY_EVEN;
	} else if (!strcmp(str, "mark")) {
		retconfig->parity = SERIAL_PARITY_MARK;
	} else if (!strcmp(str, "space")) {
		retconfig->parity = SERIAL_PARITY_SPACE;
	} else {
		logm_err("Invalid parity type %s. Only none/odd/even/mark/space are valid\n", str);
		return UFP_JSON_DATA_INVALID;
	}

	STATUS_CHECK_RET(json_read_str(match, "flow-control", &str, "none"));

	if (!strcmp(str, "none")) {
		/* Nothing to do */
	} else if (!strcmp(str, "dtr/dsr")) {
		retconfig->flags |= SERIAL_F_DTR_DSR;
	} else if (!strcmp(str, "rts/cts")) {
		retconfig->flags |= SERIAL_F_RTS_CTS;
	} else {
		logm_err("Invalid flow-control type %s. Only none/\"dtr/dsr\"/\"rts/cts\" are valid\n", str);
		return UFP_JSON_DATA_INVALID;
	}

	return UFP_OK;
}

static ufprog_status serprog_open_port(struct ufprog_interface *dev, const char *name,
				       const struct serial_port_config *config)
{
	ufprog_status ret;

	STATUS_CHECK_RET(serial_port_open(name, &dev->port));
	STATUS_CHECK_GOTO_RET(serial_port_set_config(dev->port, config), ret, out);

	return UFP_OK;

out:
	serial_port_close(dev->port);
	dev->port = NULL;

	return ret;
}

static int UFPROG_API serprog_try_match_open(void *priv, struct json_object *match, int index)
{
	struct ufprog_interface *dev = priv;
	struct serial_port_config config;
	ufprog_status ret;
	const char *port;

	ret = serprog_config_from_match(match, &port, &config);
	if (ret) {
		logm_warn("Failed to parse config from match#%u\n", index);
		return 0;
	}

	ret = serprog_open_port(dev, port, &config);
	if (ret) {
		logm_warn("Failed to open port described by match#%u\n", index);
		return 0;
	}

	dev->path = os_strdup(port);
	if (!dev->path) {
		serial_port_close(dev->port);
		dev->port = NULL;
		return 0;
	}

	return 1;
}

ufprog_status UFPROG_API ufprog_device_open(uint32_t if_type, struct json_object *config, ufprog_bool thread_safe,
					    struct ufprog_interface **outifdev)
{
	struct ufprog_interface *dev;
	ufprog_status ret;

	if (!outifdev)
		return UFP_INVALID_PARAMETER;

	*outifdev = NULL;

	if (if_type != IF_SPI)
		return UFP_UNSUPPORTED;

	if (!config) {
		logm_err("Device connection configuration required\n");
		return UFP_DEVICE_MISSING_CONFIG;
	}

	dev = calloc(1, sizeof(*dev));
	if (!dev) {
		logm_err("No memory for device object\n");
		return UFP_NOMEM;
	}

	STATUS_CHECK_RET(json_read_uint32(config, "buffer-size", &dev->buffer_size, SERPROG_MAX_BUFFER_SIZE));

	if (dev->buffer_size > SERPROG_MAX_BUFFER_SIZE)
		dev->buffer_size = SERPROG_MAX_BUFFER_SIZE;

	STATUS_CHECK_RET(json_read_uint32(config, "timeout-ms", &dev->timeout_ms, SERPROG_DEFAULT_TIMEOUT_MS));

	STATUS_CHECK_GOTO_RET(json_array_foreach(config, "match", serprog_try_match_open, dev, NULL), ret, cleanup);

	if (!dev->port) {
		logm_errdbg("No matched device opened\n");
		ret = UFP_DEVICE_NOT_FOUND;
		goto cleanup;
	}

	if (thread_safe) {
		if (!os_create_mutex(&dev->lock)) {
			logm_err("Failed to create lock for thread-safe");
			ret = UFP_LOCK_FAIL;
			goto cleanup;
		}
	}

	logm_info("Opened serial port device %s\n", dev->path);

	STATUS_CHECK_GOTO_RET(serprog_spi_init(dev), ret, cleanup);

	*outifdev = dev;
	return UFP_OK;

cleanup:
	if (dev->port)
		serial_port_close(dev->port);

	if (dev->path)
		free((void *)dev->path);

	if (dev->lock)
		os_free_mutex(dev->lock);

	free(dev);

	return ret;
}

ufprog_status UFPROG_API ufprog_device_free(struct ufprog_interface *dev)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	serial_port_close(dev->port);

	free((void *)dev->path);

	os_free_mutex(dev->lock);

	free(dev);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_device_lock(struct ufprog_interface *dev)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (!dev->lock)
		return UFP_OK;

	return os_mutex_lock(dev->lock) ? UFP_OK : UFP_LOCK_FAIL;
}

ufprog_status UFPROG_API ufprog_device_unlock(struct ufprog_interface *dev)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (!dev->lock)
		return UFP_OK;

	return os_mutex_unlock(dev->lock) ? UFP_OK : UFP_LOCK_FAIL;
}

uint32_t UFPROG_API ufprog_plugin_api_version(void)
{
	return MAKE_VERSION(SERPROG_DRV_API_VER_MAJOR, SERPROG_DRV_API_VER_MINOR);
}
