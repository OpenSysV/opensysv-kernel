/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1982,1986,1988 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * kernel/os/kern_shutdown.c
 *
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
#include <sys/proc.h>
#include <sys/uadmin.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>

#define UADMIN_SYNC 0
#define UADMIN_UMOUNT 1

STATIC void live_proc_shutdown(void);
STATIC void shutdown_proc(struct proc *p);
STATIC void dis_vfs(int op);

int	waittime = -1;

/*
 * boot() replaces uadmin()'s role in powering down and rebooting
 * the system.
 */
void
boot(int howto)
{
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		struct buf *bp;
		int iter, nbusy;

		waittime = 0;
		(void) splnet();
		cmn_err(CE_CONT, "syncing disks... ");

		/*
		 * Release vnodes held by texts before sync.
		 */

		live_proc_shutdown();	/* handle live processes. */

		dis_vfs(UADMIN_SYNC);
		dis_vfs(UADMIN_UMOUNT);

		for (iter = 0; iter < 20; iter++) {
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; )
				if ((bp->b_flags & (B_BUSY|B_INVAL)) == B_BUSY)
					nbusy++;
			if (nbusy != 0)
				cmn_err(CE_CONT, "%d ", nbusy);
			DELAY(40000 * iter);
		}
		if (nbusy)
			cmn_err(CE_CONT, "giving up\n");
		else
			cmn_err(CE_CONT, "done\n");
	}

	/*
	 * Unmount the root file system. This is called regardless if
	 * we were told to reboot or halt without syncing.
	 */
	(void) VFS_MOUNTROOT(rootvfs, ROOT_UNMOUNT);

	/*
	 * Handle halting and rebooting the system. Even though we are
	 * using the RISC-V architecture, the method used is dependent on
	 * the implementation. The functions haltsys() and rebootsys()
	 * are in machine-dependent code for each RISC-V platform.
	 */
	if (howto & RB_HALT) {
		cmn_err(CE_CONT, "System halted. You may turn off power.\n");
		haltsys();
	} else {
		cmn_err(CE_CONT, "\nAutomatic Boot Procedure\n");
		rebootsys();
	}
}

/*
 * Function:
 * live_proc_shutdown()
 *
 * Description:
 * Shut down all active (live) processes.
 *
 * Arguments:
 * None.
 *
 * Return value:
 * None.
 */
STATIC void
live_proc_shutdown(void)
{
	struct proc *p;

	/*
	 * Hold all signals so we don't die.
	 */
	sigfillset(&u.u_procp->p_hold);

	/*
	 * Kill init if it's not in a zombie state.
	 */
	if (proc_init->p_stat != SZOMB)
		shutdown_proc(proc_init);

	cmn_err(CE_CONT, "Killing all processes ");

	/*
	 * Kill all processes except kernel daemons and ourself.
	 */
	for (p = practive; p != NULL; p = p->p_next) {
		if (p->p_exec != NULLVP && p->p_stat != SZOMB && p != u.u_procp)
			shutdown_proc(p);
	}

	cmn_err(CE_CONT, "continuing\n");
}

/*
 * Function:
 * shutdown_proc()
 *
 * Description:
 * Shut down a process.
 *
 * Arguments:
 * p: Pointer to process table.
 *
 * Return value:
 * None.
 */
STATIC void
shutdown_proc(struct proc *p)
{
	struct user *pu;
	int id;

	psignal(p, SIGKILL);
	id = timeout(wakeup, (caddr_t)p, 15 * HZ);
	(void) sleep((caddr_t)p, PWAIT);

	/*
	 * Full timeout didn't elapse. Either it died properly, or we woke
	 * up prematurely; in either case, just check again later.
	 */
	if (untimeout(id) != -1)
		return;

	if (p->p_exec != NULLVP && p->p_stat != SZOMB) {
		/*
		 * Process still alive after timeout; force it to go away.
		 */
		VN_RELE(p->p_exec);
		p->p_exec = NULLVP;
		/*
		 * Assume process is hung for good; release its current
		 * and root dirs
		 */
		pu = PTOU(p);
		CATCH_FAULTS(CATCH_SEGU_FAULT) {
			VN_RELE(pu->u_cdir);
			pu->u_cdir = rootdir;
			if (pu->u_rdir) {
				VN_RELE(pu->u_rdir);
				pu->u_rdir = NULLVP;
			}
		}
		END_CATCH();
	}
}

STATIC void
dis_vfs(int op)
{
 	struct vfs *pvfsp, *cvfsp, *ovfsp;

	pvfsp = rootvfs;
	cvfsp = pvfsp->vfs_next;

	while (cvfsp != NULL) {
		ovfsp = cvfsp;

		switch (op) {
			case UADMIN_SYNC:
				(void)VFS_SYNC(cvfsp, SYNC_CLOSE, u.u_cred);
				break;
			case UADMIN_UMOUNT:
				(void)dounmount(cvfsp, u.u_cred);
				break;
			default:
				break;
		}

		cvfsp = pvfsp->vfs_next;
		if (cvfsp == ovfsp) {
			pvfsp = cvfsp;
			cvfsp = cvfsp->vfs_next;
		}
	}
}
