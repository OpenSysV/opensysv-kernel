/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 2024 Stefanos Stefanidis.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/immu.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/evecb.h>
#include <sys/hrtcntl.h>
#include <sys/hrtsys.h>
#include <sys/priocntl.h>
#include <sys/procset.h>
#include <sys/events.h>
#include <sys/evsys.h>
#include <sys/asyncsys.h>
#include <sys/var.h>
#include <sys/debug.h>
#include <sys/utsname.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <vm/as.h>
#include <vm/seg_vn.h>

/* Well known processes */
proc_t *proc_sched;		/* memory scheduler */
proc_t *proc_init;		/* init */
proc_t *proc_pageout;	/* pageout daemon */
proc_t *proc_bdflush;	/* buffer cache flush daemon */

int	physmem;	/* Physical memory size in clicks.	*/
int	maxmem;		/* Maximum available memory in clicks.	*/
int	freemem;	/* Current available memory in clicks.	*/
vnode_t	*rootdir;
extern int icode[], szicode;

/*
 * Initialization code.
 * fork - process 0 to schedule
 *      - process 1 execute bootstrap
 *
 * loop at low address in user mode -- /sbin/init
 * cannot be executed.
 */
int
main(void)
{
	int (**initptr)();
	extern int (*init_tbl[])();
	extern int sched();
	extern int pageout();
	extern int fsflush();
	extern int kmem_freepool();
	extern char bldstr[];
	proc_t *p;

	startup();
	clkstart();
	cred_init();
	dnlc_init();

	prt_where = PRW_CONS;

	/*
	 * Lets display the banner early so the user has some idea that
	 * Unix is taking over the system.
	 *
	 * Good {morning, afternoon, evening, night}.
	 */
	cmn_err(CE_CONT, "Project Elmo - v%s\n", bldstr);
	cmn_err(CE_CONT,
		"JadeOS Release %s Version %s [UNIX(R) System V Release 4.0]\n",
		utsname.release, utsname.version);

	/*
	 * Set up credentials.
	 */
	u.u_cred = crget();

	/*
	 * Call all system initialization functions.
	 */
	for (initptr= &init_tbl[0]; *initptr; initptr++) {
		cmn_err(CE_CONT, "Calling function 0x%x\n", *(int *)initptr);
		(**initptr)();
	}

	/*
	 * Set the scan rate and other parameters of the paging subsystem.
	 */
	setupclock();

	u.u_error = 0;		/* XXX kludge for SCSI driver */
	vfs_mountroot();	/* Mount the root file system */

	u.u_start = hrestime.tv_sec;

	/*
	 * This call to swapconf must come after root has been mounted.
	 */
	swapconf();

	/*
	 * Initialize file descriptor info in uarea.
	 * NB:  getf() in fio.c expects u.u_nofiles >= NFPCHUNK
	 */
	u.u_nofiles = NFPCHUNK;

	/*
	 * Kick off timeout driven events by calling first time.
	 */
	schedpaging();

	/*
	 * Make init process.
	 */
	if (newproc(NP_INIT, NULL, &error)) {
		p = u.u_procp;
		proc_init = p;

		/* 
		 * We will start the user level init.
		 * Clear the special flags set to get
		 * past the first context switch.
		 */
		p->p_flag &= ~(SSYS | SLOCK);
		p->p_cstime = p->p_stime = p->p_cutime = p->p_utime = 0;

		/*
		 * Set up the text region to do an exec of /sbin/init.
		 * The "icode" is in riscv/icode.s.
		 */

		/*
		 * Allocate user address space.
		 */
		p->p_as = as_alloc();
		if (p->p_as == NULL)
			cmn_err(CE_PANIC, "main: as_alloc() returned null!");

		/*
		 * Make a text segment for icode.
		 */
		(void) as_map(p->p_as, UVTEXT, szicode, segvn_create,
			zfod_argsp);

		if (copyout((caddr_t)icode, (caddr_t)(UVTEXT), szicode))
			cmn_err(CE_PANIC, "main: copyout of icode failed");

		return UVTEXT;
	}

	if (newproc(NP_SYSPROC, NULL, &error)) {
		proc_pageout = p;
		u.u_procp->p_cstime = u.u_procp->p_stime = 0;
		u.u_procp->p_cutime = u.u_procp->p_utime = 0;
		bcopy("pageout", u.u_psargs, 8);
		bcopy("pageout", u.u_comm, 7);
		pageout();
		cmn_err(CE_PANIC, "main: return from pageout()");
	}

	if (newproc(NP_SYSPROC, NULL, &error)) {
		proc_bdflush = p;
		u.u_procp->p_cstime = u.u_procp->p_stime = 0;
		u.u_procp->p_cutime = u.u_procp->p_utime = 0;
		bcopy("fsflush", u.u_psargs, 8);
		bcopy("fsflush", u.u_comm, 7);
		fsflush();
		cmn_err(CE_PANIC, "main: return from fsflush()");
	}

	if (aio_config() && aiodmn_spawn() != 0) {
		aiodaemon();
		cmn_err(CE_PANIC, "main: return from aiodaemon()");
	}

	if (newproc(NP_SYSPROC, NULL, &error)) {
		/*
		 * use "kmdaemon" rather than "kmem_freepool"
		 * will be more intelligble for ps
		 */
		u.u_procp->p_cstime = u.u_procp->p_stime = 0;
		u.u_procp->p_cutime = u.u_procp->p_utime = 0;
		bcopy("kmdaemon", u.u_psargs, 10);
		bcopy("kmdaemon", u.u_comm, 9);
		kmem_freepool();
		cmn_err(CE_PANIC, "main: return from kmem_freepool()");
	}

	pid_setmin();

	/*
	 * Enter scheduling loop with system process.
	 */
	bcopy("sched", u.u_psargs, 6);
	bcopy("sched", u.u_comm, 5);
	return((int)sched);
}
