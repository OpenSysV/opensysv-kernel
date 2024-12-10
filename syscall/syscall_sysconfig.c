/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysconfig.h>

struct sysconfiga {
	int which;
};

int
sysconfig(struct sysconfiga *uap, rval_t *rvp)
{
	switch (uap->which) {
		case _CONFIG_CLK_TCK:
			rvp->r_val1 = HZ;
			break;

		case _CONFIG_NGROUPS:
			/*
			 * Maximum number of supplementary groups.
			 */
			rvp->r_val1 = ngroups_max;
			break;

		case _CONFIG_OPEN_FILES:
			/*
			 * Maxiumum number of open files (soft limit).
			 */
			rvp->r_val1 = u.u_rlimit[RLIMIT_NOFILE].rlim_cur;
			break;

		case _CONFIG_CHILD_MAX:
			/*
			 * Maxiumum number of processes.
			 */
			rvp->r_val1 = v.v_maxup;
			break;

		case _CONFIG_POSIX_VER:
			rvp->r_val1 = _POSIX_VERSION;	/* current POSIX version */
			break;
	
		case _CONFIG_PAGESIZE:
			rvp->r_val1 = PAGESIZE;
			break;

		case _CONFIG_XOPEN_VER:
			rvp->r_val1 = _XOPEN_VERSION; /* current XOPEN version */
			break;

		default:
			return EINVAL;
	}
	
	return 0;
}
