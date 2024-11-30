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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/uadmin.h>
#include <sys/statvfs.h>
#include <sys/reboot.h>

struct uadmina {
	int cmd;
	int fcn;
	int mdep;
};

/*
 * Administrative system call.
 */
int
uadmin(struct uadmina *uap, rval_t *rvp)
{
	static int ualock;
	int error = 0;

	switch (uap->cmd) {
		case A_SHUTDOWN:
		case A_REBOOT:
		case A_REMOUNT:
		case A_SWAPCTL:
		case A_CLOCK:
		case A_SETCONFIG:
			break;
		default:
			return EINVAL;
	}

	if (uap->cmd != A_SWAPCTL) {
		if (!suser(u.u_cred))
			return EPERM;
		if (ualock)
			return 0;
		ualock = 1;
	}

	switch (uap->cmd) {
		case A_SHUTDOWN:
		case A_REBOOT:
			switch (uap->fcn) {
				case AD_HALT:
					boot(RB_HALT);
					break;
				case AD_IBOOT:
				case AD_BOOT:
					boot(RB_AUTOBOOT);
					break;
				default:
					cmn_err(CE_CONT, "uadmin: Invalid argument.\n");
					return EINVAL;
			}
			break;

		case A_REMOUNT:
			(void) VFS_MOUNTROOT(rootvfs, ROOT_REMOUNT);
			break;

		case A_SWAPCTL:
			error = swapctl(uap, rvp);
			break;

		/*
		 * Correction (in seconds) from local time to gmt is passed.
		 * This is used by wtodc() when called later from stime()
		 * so that rtc is in local time.
		 * Nothing else is done right now in order to avoid needing
		 * code in kernel to convert rtc format to internal format.
		 * That can be done at user level during startup.
		 */
		case A_CLOCK:
			c_correct = uap->fcn;
			break;

		case A_SETCONFIG:
			switch (uap->fcn) {
				case AD_PANICBOOT:
					bootpanic = (int) uap->mdep;
					break;
				default:
					error = EINVAL;
					break;
			}
			break;
	}

	if (uap->cmd != A_SWAPCTL)
		ualock = 0;
	return error;
}
