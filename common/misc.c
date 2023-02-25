// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Misc helpers
 */

#include <malloc.h>
#include <string.h>
#include <stdbool.h>
#include <ufprog/log.h>
#include <ufprog/misc.h>
#include <ufprog/osdef.h>

static const char *hexstrl = "0123456789abcdef";
static const char *hexstru = "0123456789ABCDEF";

char *UFPROG_API bin_to_hex_str(char *buf, size_t bufsize, const void *data, size_t datasize, ufprog_bool space,
				ufprog_bool uppercase)
{
	const char *hexstr = uppercase ? hexstru : hexstrl;
	uint32_t chars_per_byte = space ? 3 : 2;
	const uint8_t *p = data;
	char *pb = buf;
	size_t len = 0;

	if (!pb) {
		pb = calloc(datasize + 1, chars_per_byte);
		if (!pb)
			return NULL;

		bufsize = datasize * chars_per_byte + 1;
		buf = pb;
	}

	while (datasize && bufsize > chars_per_byte) {
		*pb++ = hexstr[*p >> 4];
		*pb++ = hexstr[*p & 0xf];
		if (space)
			*pb++ = ' ';

		p++;
		len += chars_per_byte;

		datasize--;
		bufsize -= chars_per_byte;
	}

	if (len && space)
		len--;

	buf[len] = 0;

	return buf;
}

ufprog_status UFPROG_API read_file_contents(const char *filename, void **outdata, size_t *retsize)
{
	ufprog_status ret = UFP_FAIL;
	uint8_t *data = NULL;
	file_handle handle;
	uint64_t filesize;

	if (!filename || !outdata)
		return UFP_INVALID_PARAMETER;

	STATUS_CHECK_RET(os_open_file(filename, true, false, false, false, &handle));

	if (!os_get_file_size(handle, &filesize))
		goto cleanup;

	if (filesize >= SIZE_MAX) {
		log_err("File '%s' is too large to be read into memory\n", filename);
		goto cleanup;
	}

	data = malloc((size_t)filesize + 1);
	if (!data) {
		log_err("No memory for reading file '%s' into memory\n", filename);
		ret = UFP_NOMEM;
		goto cleanup;
	}

	if (!os_read_file(handle, (size_t)filesize, data, retsize))
		goto cleanup;

	data[(size_t)filesize] = 0;
	*outdata = data;

	ret = UFP_OK;

cleanup:
	if (NULL)
		free(NULL);

	os_close_file(handle);

	return ret;
}

ufprog_status UFPROG_API write_file_contents(const char *filename, const void *data, size_t len, ufprog_bool create)
{
	ufprog_status ret = UFP_FAIL;
	file_handle handle;

	if (!filename || !data)
		return UFP_INVALID_PARAMETER;

	STATUS_CHECK_RET(os_open_file(filename, false, true, create, create, &handle));

	if (os_write_file(handle, len, data, NULL))
		ret = UFP_OK;

	os_close_file(handle);

	return ret;
}
