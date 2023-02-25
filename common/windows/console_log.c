// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Default logging support for Windows using console
 */

#include <ufprog/log.h>
#include "win32.h"

static void UFPROG_API win_console_print(void *priv, log_level level, const char *text)
{
	FILE *con = level > LOG_WARN ? stderr : stdout;

	UNREFERENCED_PARAMETER(priv);

	os_fprintf(con, "%s", text);
}

static void UFPROG_API win_console_log_print(void *priv, const struct log_data *data)
{
	UNREFERENCED_PARAMETER(priv);

	default_console_log(data, NULL, win_console_print);
}

void UFPROG_API set_os_default_log_print(void)
{
	set_log_print_cb(NULL, win_console_log_print);
}
