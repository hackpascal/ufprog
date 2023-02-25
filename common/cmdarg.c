// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Command argument helper
 */

#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ufprog/osdef.h>
#include <ufprog/cmdarg.h>

ufprog_status UFPROG_API dispatch_subcmd(const struct subcmd_entry *entries, uint32_t count, void *priv,
					 int argc, char *argv[], int *cmdret)
{
	int ret;

	while (count && argc) {
		if (!strcmp(argv[0], entries->name)) {
			ret = (*entries->cmd)(priv, argc, argv);

			if (cmdret)
				*cmdret = ret;

			return UFP_OK;
		}

		entries++;
		count--;
	}

	return UFP_NOT_EXIST;
}

ufprog_status UFPROG_API cmdarg_parse(struct cmdarg_entry *entries, uint32_t count, int argc, char *argv[],
				      int *next_argc, uint32_t *erridx, int *errarg)
{
	ufprog_status ret = UFP_CMDARG_INVALID_VALUE;
	char *entval, *end;
	int argc_curr = 0;
	bool curr_hit;
	size_t entlen;
	uint32_t i;

	union {
		uint64_t u64;
		int64_t s64;
	} val;

	if (!count)
		return UFP_OK;

	if (!entries || !argv)
		return UFP_INVALID_PARAMETER;

	for (i = 0; i < count; i++) {
		if (entries[i].set)
			*entries[i].set = false;
	}

	while (argc_curr < argc) {
		curr_hit = false;

		for (i = 0; i < count; i++) {
			entlen = strlen(entries[i].name);

			if (strncmp(argv[argc_curr], entries[i].name, entlen))
				continue;

			if (argv[argc_curr][entlen] == '=') {
				entval = &argv[argc_curr][entlen + 1];
				if (!*entval)
					entval = NULL;
			} else if (argv[argc_curr][entlen] == 0) {
				entval = NULL;
			} else {
				continue;
			}

			curr_hit = true;

			if (entries[i].type == CMDARG_BOOL) {
				if (!entval) {
					*entries[i].value.b = true;
				} else {
					val.u64 = strtoull(entval, &end, 0);
					if (end > entval && !*end) {
						*entries[i].value.b = !!val.u64;
					} else {
						if (!strcasecmp(entval, "yes") || !strcasecmp(entval, "true"))
							*entries[i].value.b = true;
						else if (!strcasecmp(entval, "no") || !strcasecmp(entval, "false"))
							*entries[i].value.b = false;
						else
							goto errout;
					}
				}

				goto curr_processed;
			}

			if (!entval) {
				ret = UFP_CMDARG_MISSING_VALUE;
				goto errout;
			}

			if (entries[i].type == CMDARG_STRING) {
				*entries[i].value.s = entval;
				goto curr_processed;
			}

			switch (entries[i].type) {
			case CMDARG_S8:
			case CMDARG_S16:
			case CMDARG_S32:
			case CMDARG_S64:
			case CMDARG_INTPTR:
				val.s64 = strtoll(entval, &end, 0);
				if (end == entval || *end || val.s64 == LLONG_MAX)
					goto errout;
				break;

			case CMDARG_U8:
			case CMDARG_U16:
			case CMDARG_U32:
			case CMDARG_U64:
			case CMDARG_UINTPTR:
				val.u64 = strtoull(entval, &end, 0);
				if (end == entval || *end || val.u64 == ULLONG_MAX)
					goto errout;
				break;

			default:
				ret = UFP_CMDARG_INVALID_TYPE;
				goto errout;
			}

			switch (entries[i].type) {
			case CMDARG_S8:
				if (val.s64 < INT8_MIN || val.s64 > INT8_MAX)
					goto errout;

				*entries[i].value.s8 = (int8_t)val.s64;
				break;

			case CMDARG_U8:
				if (val.u64 > UINT8_MAX)
					goto errout;

				*entries[i].value.u8 = (uint8_t)val.u64;
				break;

			case CMDARG_S16:
				if (val.s64 < INT16_MIN || val.s64 > INT16_MAX)
					goto errout;

				*entries[i].value.s16 = (int16_t)val.s64;
				break;

			case CMDARG_U16:
				if (val.u64 > UINT16_MAX)
					goto errout;

				*entries[i].value.u16 = (uint16_t)val.u64;
				break;

			case CMDARG_S32:
				if (val.s64 < INT32_MIN || val.s64 > INT32_MAX)
					goto errout;

				*entries[i].value.s32 = (int32_t)val.s64;
				break;

			case CMDARG_U32:
				if (val.u64 > UINT32_MAX)
					goto errout;

				*entries[i].value.u32 = (uint32_t)val.u64;
				break;

			case CMDARG_S64:
				if (val.s64 < INT64_MIN || val.s64 > INT64_MAX)
					goto errout;

				*entries[i].value.s64 = (int64_t)val.s64;
				break;

			case CMDARG_U64:
				if (val.u64 > UINT64_MAX)
					goto errout;

				*entries[i].value.u64 = (uint64_t)val.u64;
				break;

			case CMDARG_INTPTR:
				if (val.s64 < INTPTR_MIN || val.s64 > INTPTR_MAX)
					goto errout;

				*entries[i].value.sptr = (intptr_t)val.s64;
				break;

			case CMDARG_UINTPTR:
				if (val.u64 > UINTPTR_MAX)
					goto errout;

				*entries[i].value.uptr = (uintptr_t)val.u64;
				break;
			}

		curr_processed:
			if (entries[i].set)
				*entries[i].set = true;

			break;
		}

		if (!curr_hit)
			break;

		argc_curr++;
	}

	if (next_argc)
		*next_argc = argc_curr;

	return UFP_OK;

errout:
	if (erridx)
		*erridx = i;

	if (errarg)
		*errarg = argc_curr;

	return ret;
}
