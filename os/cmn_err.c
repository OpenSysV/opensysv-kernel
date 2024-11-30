/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 2024 Stefanos Stefanidis.
 * All rights reserved.
 */

#include <sys/cmn_err.h>
#include <sys/reboot.h>

int bootopt = RB_AUTOBOOT;

static void xcmn_err(int level, char **fmtp);

void
cmn_err(int level, char *fmt, ...)
{
	xcmn_err(level, &fmt);
}

static void
xcmn_err(int level, char **fmtp)
{
	char *fmt = *fmtp;
	u_int nextarg = (u_int) ((u_int *)fmtp + 1);

	/*
	 * Set up to print to putbuf, console, or both as indicated by the
	 * first character of the format.
	 */

	if (*fmt == '!') {
		prt_where = PRW_BUF;
		if (prt_flag)
			fmt++;
	} else if (*fmt == '^') {
		prt_where = PRW_CONS;
		if (prt_flag)
			fmt++;
	} else
		prt_where = PRW_BUF | PRW_CONS;

	switch (level) {
		case CE_CONT:
			errprintf(fmt, nextarg);
			break;

		case CE_NOTE:
			errprintf("\nNOTICE: ");
			errprintf(fmt, nextarg);
			errprintf("\n");
			break;

		case CE_WARN:
			errprintf("\nWARNING: ");
			errprintf(fmt, nextarg);
			errprintf("\n");
			break;

		case CE_PANIC:
			switch (panic_level) {
				case 0:
					/*
					 * Processes logging console messages
					 * will never run.  Force message to
					 * go to console.
					 */
					conslogging = 0;
					prt_where = PRW_CONS | PRW_BUF;
					panic_level = 1;
					printf("\nPANIC: ");
					xprintf(fmt, nextarg);
					printf("\n");
					break;

				case 1:
					prt_where = PRW_CONS | PRW_BUF;
					bootopt |= RB_NOSYNC;
					panic_level = 2;
					printf("\nDOUBLE PANIC: ");
					xprintf(fmt, nextarg);
					printf("\n");
					break;

				default:
					boot(bootopt);
			}

			panicstr = fmt;	/* side-effects */
			boot(bootopt);
			splhi();

		default:
			cmn_err(CE_PANIC, "cmn_err(%d, %s) unknown.\n", level, fmt);
	}
}
