// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ONFI parameter page helpers
 */

#include <ufprog/bits.h>
#include <ufprog/nand.h>
#include <ufprog/onfi-param-page.h>

const char *UFPROG_API ufprog_onfi_revision(const void *pp)
{
	uint16_t rev = ufprog_pp_read_u16(pp, ONFI_REVISION_OFFS);

	if (rev & ONFI_REVISION_4_2)
		return "4.2";

	if (rev & ONFI_REVISION_4_1)
		return "4.1";

	if (rev & ONFI_REVISION_4_0)
		return "4.0";

	if (rev & ONFI_REVISION_3_2)
		return "3.2";

	if (rev & ONFI_REVISION_3_1)
		return "3.1";

	if (rev & ONFI_REVISION_3_0)
		return "3.0";

	if (rev & ONFI_REVISION_2_3)
		return "2.3";

	if (rev & ONFI_REVISION_2_2)
		return "2.2";

	if (rev & ONFI_REVISION_2_1)
		return "2.1";

	if (rev & ONFI_REVISION_2_0)
		return "2.0";

	if (rev & ONFI_REVISION_1_0)
		return "1.0";

	return "[Unknown revision]";
}
