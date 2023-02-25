/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Windows-specific entrypoint translation
 */

#include <malloc.h>
#include <ufprog/osdef.h>
#include "win32.h"

int os_main(os_main_entry entry, int argc, wchar_t *argv[])
{
	char **utf8_argv;
	int i, ret = -1;

	utf8_argv = calloc(argc + 1, sizeof(char *));
	if (!utf8_argv)
		return -1;

	for (i = 0; i < argc; i++) {
		utf8_argv[i] = wcs_to_utf8(argv[i]);
		if (!utf8_argv[i])
			goto cleanup;
	}

	ret = entry(argc, utf8_argv);

cleanup:
	for (i = 0; i < argc; i++) {
		if (utf8_argv[i])
			free(utf8_argv[i]);
	}

	return ret;
}
