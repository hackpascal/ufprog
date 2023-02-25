/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Command argument helper
 */
#pragma once

#ifndef _UFPROG_CMDARG_H_
#define _UFPROG_CMDARG_H_

#include <stdint.h>
#include <ufprog/common.h>

EXTERN_C_BEGIN

struct subcmd_entry {
	const char *name;
	int (*cmd)(void *priv, int argc, char *argv[]);
};

#define SUBCMD(_name, _fn) { .name = (_name), .cmd = (_fn) }

ufprog_status UFPROG_API dispatch_subcmd(const struct subcmd_entry *entries, uint32_t count, void *priv,
					 int argc, char *argv[], int *cmdret);

enum cmdarg_type {
	CMDARG_BOOL,
	CMDARG_S8,
	CMDARG_U8,
	CMDARG_S16,
	CMDARG_U16,
	CMDARG_S32,
	CMDARG_U32,
	CMDARG_S64,
	CMDARG_U64,
	CMDARG_INTPTR,
	CMDARG_UINTPTR,
	CMDARG_STRING,

	__CMDARG_TYPE_MAX
};

struct cmdarg_entry {
	uint8_t /* enum cmdarg_type */ type;
	const char *name;

	ufprog_bool *set;
	union {
		ufprog_bool *b;
		int8_t *s8;
		uint8_t *u8;
		int16_t *s16;
		uint16_t *u16;
		int32_t *s32;
		uint32_t *u32;
		int64_t *s64;
		uint64_t *u64;
		intptr_t *sptr;
		uintptr_t *uptr;
		char **s;
	} value;
};

#define CMDARG_BOOL_OPT(_name, _retval) \
	{ .type = CMDARG_BOOL, .name = (_name), .value.b = &(_retval) }

#define CMDARG_BOOL_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_BOOL, .name = (_name), .value.b = &(_retval), .set = &(_set) }

#define CMDARG_U8_OPT(_name, _retval) \
	{ .type = CMDARG_U8, .name = (_name), .value.u8 = &(_retval) }

#define CMDARG_U8_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_U8, .name = (_name), .value.u8 = &(_retval), .set = &(_set) }

#define CMDARG_S8_OPT(_name, _retval) \
	{ .type = CMDARG_U8, .name = (_name), .value.s8 = &(_retval) }

#define CMDARG_S8_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_U8, .name = (_name), .value.s8 = &(_retval), .set = &(_set) }

#define CMDARG_U16_OPT(_name, _retval) \
	{ .type = CMDARG_U16, .name = (_name), .value.u16 = &(_retval) }

#define CMDARG_U16_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_U16, .name = (_name), .value.u16 = &(_retval), .set = &(_set) }

#define CMDARG_S16_OPT(_name, _retval) \
	{ .type = CMDARG_U16, .name = (_name), .value.s16 = &(_retval) }

#define CMDARG_S16_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_U16, .name = (_name), .value.s16 = &(_retval), .set = &(_set) }

#define CMDARG_U32_OPT(_name, _retval) \
	{ .type = CMDARG_U32, .name = (_name), .value.u32 = &(_retval) }

#define CMDARG_U32_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_U32, .name = (_name), .value.u32 = &(_retval), .set = &(_set) }

#define CMDARG_S32_OPT(_name, _retval) \
	{ .type = CMDARG_U32, .name = (_name), .value.s32 = &(_retval) }

#define CMDARG_S32_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_U32, .name = (_name), .value.s32 = &(_retval), .set = &(_set) }

#define CMDARG_U64_OPT(_name, _retval) \
	{ .type = CMDARG_U64, .name = (_name), .value.u64 = &(_retval) }

#define CMDARG_U64_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_U64, .name = (_name), .value.u64 = &(_retval), .set = &(_set) }

#define CMDARG_S64_OPT(_name, _retval) \
	{ .type = CMDARG_U64, .name = (_name), .value.s64 = &(_retval) }

#define CMDARG_S64_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_U64, .name = (_name), .value.s64 = &(_retval), .set = &(_set) }

#define CMDARG_UINTPTR_OPT(_name, _retval) \
	{ .type = CMDARG_UINTPTR, .name = (_name), .value.uptr = &(_retval) }

#define CMDARG_UINTPTR_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_UINTPTR, .name = (_name), .value.uptr = &(_retval), .set = &(_set) }

#define CMDARG_INTPTR_OPT(_name, _retval) \
	{ .type = CMDARG_U8, .name = (_name), .value.sptr = &(_retval) }

#define CMDARG_INTPTR_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_U8, .name = (_name), .value.sptr = &(_retval), .set = &(_set) }

#define CMDARG_STRING_OPT(_name, _retval) \
	{ .type = CMDARG_STRING, .name = (_name), .value.s = &(_retval) }

#define CMDARG_STRING_OPT_SET(_name, _retval, _set) \
	{ .type = CMDARG_STRING, .name = (_name), .value.s = &(_retval), .set = &(_set) }

ufprog_status UFPROG_API cmdarg_parse(struct cmdarg_entry *entries, uint32_t count, int argc, char *argv[],
				      int *next_argc, uint32_t *erridx, int *errarg);

EXTERN_C_END

#endif /* _UFPROG_CMDARG_H_ */
