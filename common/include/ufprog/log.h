/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Logging support
 */
#pragma once

#ifndef _UFPROG_LOG_H_
#define _UFPROG_LOG_H_

#include <stdarg.h>
#include <ufprog/common.h>
#include <ufprog/osdef.h>

EXTERN_C_BEGIN

typedef enum log_level {
	LOG_DEBUG,
	LOG_ERR_DEBUG,
	LOG_NOTICE,
	LOG_INFO,
	LOG_WARN,
	LOG_ERR,

	__MAX_LOG_LEVEL
} log_level;

#define DEFAULT_LOG_LEVEL			LOG_INFO

struct log_data {
	log_level level;
	const char *module;
	const char *body;
};

/* Application should set the log print callback to their own implementation */
typedef void (UFPROG_API *log_print_t)(void *priv, const struct log_data *data);

ufprog_status UFPROG_API set_log_print_cb(void *priv, log_print_t fn);

/* Useful functions provided by this library */
log_level UFPROG_API set_log_print_level(log_level level);

ufprog_status UFPROG_API log_print(log_level level, const char *module, const char *text);
ufprog_status UFPROG_API log_vprintf(log_level level, const char *module, const char *fmt, va_list args);
ufprog_status log_printf(log_level level, const char *module, const char *fmt, ...);

typedef void (UFPROG_API *console_print_t)(void *priv, log_level level, const char *text);
ufprog_status UFPROG_API default_console_log(const struct log_data *data, void *priv, console_print_t cprint);

#define log_dbg(...)		log_printf(LOG_DEBUG, NULL, __VA_ARGS__)
#define log_errdbg(...)		log_printf(LOG_ERR_DEBUG, NULL, __VA_ARGS__)
#define log_notice(...)		log_printf(LOG_NOTICE, NULL, __VA_ARGS__)
#define log_info(...)		log_printf(LOG_INFO, NULL, __VA_ARGS__)
#define log_warn(...)		log_printf(LOG_WARN, NULL, __VA_ARGS__)
#define log_err(...)		log_printf(LOG_ERR, NULL, __VA_ARGS__)

#ifdef UFP_MODULE_NAME
#define logm_dbg(...)		log_printf(LOG_DEBUG, UFP_MODULE_NAME, __VA_ARGS__)
#define logm_errdbg(...)	log_printf(LOG_ERR_DEBUG, UFP_MODULE_NAME, __VA_ARGS__)
#define logm_notice(...)	log_printf(LOG_NOTICE, UFP_MODULE_NAME, __VA_ARGS__)
#define logm_info(...)		log_printf(LOG_INFO, UFP_MODULE_NAME, __VA_ARGS__)
#define logm_warn(...)		log_printf(LOG_WARN, UFP_MODULE_NAME, __VA_ARGS__)
#define logm_err(...)		log_printf(LOG_ERR, UFP_MODULE_NAME, __VA_ARGS__)
#else
#define logm_dbg		log_dbg
#define logm_errdbg		log_errdbg
#define logm_notice		log_notice
#define logm_info		log_info
#define logm_warn		log_warn
#define logm_err		log_err
#endif

EXTERN_C_END

#endif /* _UFPROG_LOG_H_ */
