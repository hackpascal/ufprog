// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Logging support
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <ufprog/log.h>

/* Log text prefix */
static const char *log_prefixes[] = {
	[LOG_DEBUG] = "(Debug)",
	[LOG_ERR_DEBUG] = "(Error)",
	[LOG_NOTICE] = "Notice:",
	[LOG_INFO] = NULL,
	[LOG_WARN] = "[WARN]",
	[LOG_ERR] = "[ERROR]",
};

/* Default log level is INFO */
static log_level current_log_level = DEFAULT_LOG_LEVEL;

/* Registered log callback */
static log_print_t log_print_fn;
static void *log_print_priv;

ufprog_status UFPROG_API set_log_print_cb(void *priv, log_print_t fn)
{
	if (!fn)
		return UFP_INVALID_PARAMETER;

	log_print_fn = fn;
	log_print_priv = priv;

	return UFP_OK;
}

log_level UFPROG_API set_log_print_level(log_level level)
{
	log_level old_level = current_log_level;

	if (level < 0 || level >= __MAX_LOG_LEVEL)
		return old_level;

	current_log_level = level;

	return old_level;
}

ufprog_status UFPROG_API log_print(log_level level, const char *module, const char *text)
{
	struct log_data ld;

	if (!text)
		return UFP_OK;

	if (!log_print_fn)
		return UFP_OK;

	if (level >= __MAX_LOG_LEVEL)
		return UFP_INVALID_PARAMETER;

	if (level < current_log_level)
		return UFP_OK;

	ld.level = level;
	ld.module = module;
	ld.body = text;

	log_print_fn(log_print_priv, &ld);

	return UFP_OK;
}

ufprog_status UFPROG_API log_vprintf(log_level level, const char *module, const char *fmt, va_list args)
{
	ufprog_status ret = UFP_OK;
	char *buf = NULL;
	va_list args_cpy;
	int len;

	if (!fmt)
		return UFP_OK;

	if (!log_print_fn)
		return UFP_OK;

	if (level >= __MAX_LOG_LEVEL)
		return UFP_INVALID_PARAMETER;

	if (level < current_log_level)
		return UFP_OK;

	va_copy(args_cpy, args);
	len = vsnprintf(NULL, 0, fmt, args);

	if (len < 0) {
		ret = UFP_FAIL;
		goto out;
	}

	buf = malloc(len + 1);
	if (!buf) {
		ret = UFP_NOMEM;
		goto out;
	}

	len = vsnprintf(buf, len + 1, fmt, args_cpy);
	if (len < 0) {
		ret = UFP_FAIL;
		goto out;
	}

	ret = log_print(level, module, buf);

out:
	if (buf)
		free(buf);

	return ret;
}

ufprog_status log_printf(log_level level, const char *module, const char *fmt, ...)
{
	ufprog_status ret;
	va_list args;

	va_start(args, fmt);
	ret = log_vprintf(level, module, fmt, args);
	va_end(args);

	return ret;
}

ufprog_status UFPROG_API default_console_log(const struct log_data *data, void *priv, console_print_t cprint)
{
	size_t len, prefix_len = 0, module_name_len = 0, body_len;
	const char *prefix;
	char *buf, *p;

	if (!data || !cprint)
		return UFP_INVALID_PARAMETER;

	prefix = log_prefixes[data->level];
	if (prefix)
		prefix_len = strlen(prefix) + 1;

	if (data->module)
		module_name_len = strlen(data->module) + 2;

	body_len = strlen(data->body);

	len = prefix_len + module_name_len + body_len;

	buf = malloc(len + 1);
	if (!buf)
		return UFP_NOMEM;

	p = buf;

	if (prefix_len) {
		snprintf(p, buf - p + len + 1, "%s ", prefix);
		p += prefix_len;
	}

	if (module_name_len) {
		snprintf(p, buf - p + len + 1, "%s: ", data->module);
		p += module_name_len;
	}

	memcpy(p, data->body, body_len + 1);

	cprint(priv, data->level, buf);

	free(buf);

	return UFP_OK;
}
