/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
 * Copyright (c) 2024, 2025 Stefanos Stefanidis.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/varargs.h>	/* Internal variant of stdarg.h */
#include <sys/cmn_err.h>
#include <sys/reboot.h>

int bootopt = RB_AUTOBOOT;

STATIC void xcmn_err(int level, char *fmt, va_list adx);
STATIC void do_panic(char *fmt, va_list adx);

void
cmn_err(int level, char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	xcmn_err(level, fmt, adx);
	va_end(adx);
}

STATIC void
xcmn_err(int level, char *fmt, va_list adx)
{
	/*
	 * Set up to print to putbuf, console, or both as indicated by the
	 * first character of the format.
	 */
	if (*fmt == '!') {
		prt_where = PRW_BUF;
		fmt++;
	} else if (*fmt == '^') {
		prt_where = PRW_CONS;
		fmt++;
	} else
		prt_where = PRW_BUF | PRW_CONS;

	switch (level) {
		case CE_CONT:
			prf(fmt, adx, (vnode_t *)NULL, prt_where, 0);
			break;

		case CE_NOTE:
			eprintf(fmt, adx, prt_where, SL_NOTE, "NOTICE");
			break;

		case CE_WARN:
			eprintf(fmt, adx, prt_where, SL_WARN, "WARNING");
			break;

		case CE_PANIC:
			/*
			 * Processes logging console messages will never run.
			 * Force message to go to console.
			 */
			conslogging = 0;
			do_panic(fmt, adx);
			break;

		default:
			cmn_err(CE_PANIC, "cmn_err(%d, %s) unknown.\n", level, fmt);
	}
}

STATIC void
do_panic(char *fmt, va_list adx)
{
	simple_lock(&panic_lock);

	/*
	 * Allow only threads on panicking cpu to blow through locks.
	 * Use counter to avoid endless looping if code should panic
	 * before panicstr is set.
	 */
	if (panicstr) {
		if (cpu_number() != paniccpu)
			simple_unlock(&panic_lock);
		else {
			panicstr = fmt;
			panicargs = adx;
			bootopt |= RB_NOSYNC;
		}
	} else {
		panicstr = fmt;
		panicargs = adx;
		paniccpu = cpu_number();
	}

	prt_where = PRW_CONS | PRW_BUF;
	simple_unlock(&panic_lock);

	printf("panic (cpu %u): ", paniccpu);
	prf(panicstr, panicargs, (vnode_t *)NULL, prt_where, 0);

	boot(bootopt);
}
