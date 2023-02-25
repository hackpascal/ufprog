// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Simple console progress bar
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/osdef.h>
#include <ufprog/progbar.h>

void UFPROG_API progress_init(void)
{
	char prog[] = "[                                                                        ]   0% ";

	os_printf("%s", prog);
	fflush(stdout);
}

void UFPROG_API progress_show(uint32_t percentage)
{
	char prog[] = "[                                                                        ]   0% ";
	uint32_t i, count;
	char per[5];

	if (percentage > 100)
		percentage = 100;

	count = percentage * 72 / 100;
	if (count)
		count--;

	for (i = 1; i <= count; i++)
		prog[i] = '=';
	prog[i] = '>';

	snprintf(per, sizeof(per), "%u%%", percentage);

	memcpy(prog + strlen(prog) - strlen(per) - 1, per, strlen(per));

	os_printf("\r%s", prog);
	fflush(stdout);
}

void UFPROG_API progress_done(void)
{
	char prog[] = "[========================================================================] 100% ";

	os_printf("\r%s\n", prog);
	fflush(stdout);
}
