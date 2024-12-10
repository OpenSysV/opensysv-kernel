/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * Copyright (c) 2024 Stefanos Stefanidis.
 * All rights reserved.
 */

/*
 * kernel/mach/rfs_init.c
 *
 * Remote file system - initialization module
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/user.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/cmn_err.h>
#include <mach/rfs/rfs.h>

/*
 *  rfs_init - data structure initialization
 *
 *  Initialize the parallel process table remote connection block queue
 *  headers.
 */
void
rfs_init(void)
{
	struct rfsProcessEntry *rpep;

	for (rpep=rfsProcessTable; rpep < &rfsProcessTable[nproc]; rpep++)
		initQueue(&rpep->rpe_rcbq);
}

/*
 *  rfs_initroot - local root directory initialization
 *
 *  Look for a local root directory (which will have all the remote file system
 *  link inodes installed above it).  If an appropriate local root directory is
 *  found, change the current and root directory pointers for this process to
 *  use it so that all future references above the root (e.g. /../<system-
 *  name/*) will be able to find the remote links.  If no appropriate directory
 *  is found, leave the current and root directory pointers at the physical
 *  root of the file system.
 *
 *  This routine is called very earlier in the system initialization
 *  procedure by process 0.
 *
 *  N.B.  It is usually prudent to always have symbolic link files for at least
 *  "/etc", "/bin" and "/dev" (and all other top level directories if possible)
 *  installed on the physical root.  This aids in switching between operating
 *  systems with and without support for the remote file system by permitting
 *  single-user bootstraps of either version of an operating system.  These
 *  symbolic links should also be relative to the physical root directory
 *  itself so that such a reorganized file system may be mounted when it is not
 *  the root without causing any nasty surprises.
 */
char *rfs_localroot[] = {"/RFS/.LOCALROOT", "/REM/.LOCALROOT", 0};

void
rfs_initroot()
{
}

/*
 *  rfs_oops() - catch an unexpected remote file system call dispatch
 *
 *  Indicate the system call number which invoked the error and
 *  kernel panic.
 */
void
rfs_oops(void)
{
	cmn_err(CE_PANIC, "RFS error. Invoked by syscall %d", u.u_rfscode);
}
