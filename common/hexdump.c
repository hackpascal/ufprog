// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Generic hexdump
 */

#include <stdio.h>
#include <inttypes.h>
#include <ufprog/osdef.h>
#include <ufprog/hexdump.h>

void UFPROG_API hexdump(const void *data, size_t size, uint64_t addr, ufprog_bool head_align)
{
	const uint8_t *p = data;
	uint32_t i, chklen, padding;

	if (!data)
		return;

	while (size) {
		if (head_align)
			padding = addr % 16;
		else
			padding = 0;

		chklen = 16 - padding;
		if (chklen > size)
			chklen = (uint32_t)size;

		os_printf("%08" PRIx64 ": ", addr - padding);

		for (i = 0; i < padding; i++) {
			if (i && (i % 4 == 0))
				os_printf(" ");

			os_printf("   ");
		}

		for (i = 0; i < chklen; i++) {
			if ((padding + i) && ((padding + i) % 4 == 0))
				os_printf(" ");

			os_printf("%02x ", p[i]);
		}

		for (i = padding + chklen; i < 16; i++) {
			if (i && (i % 4 == 0))
				os_printf(" ");

			os_printf("   ");
		}
		os_printf(" ");

		for (i = 0; i < padding; i++)
			os_printf(" ");

		for (i = 0; i < chklen; i++) {
			if (p[i] < 32 || p[i] >= 0x7f)
				os_printf(".");
			else
				os_printf("%c", p[i]);
		}
		os_printf("\n");

		p += chklen;
		size -= chklen;
		addr += chklen;
	}
}
