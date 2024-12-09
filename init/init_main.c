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
#include <sys/copyright.h>
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

#ifdef __i386__
STATIC void
init_finish(proc_t *p)
{
	struct dscr *ldt, *ldta;
	struct gate_desc *gldt;
	extern struct gate_desc scall_dscr, sigret_dscr;

	/*
 	 * Allocate a stack segment.
 	 */
	(void) as_map(p->p_as, userstack, ctob(SSIZE), segvn_create, zfod_argsp);

	/*
	 * 80386 B1 Errata #10 -- reserve page at 0x80000000
	 * to prevent bug from occuring.
	 */
	if (do386b1_x87)
		(void) as_map(p->p_as, 0x80000000, ctob(1), segdummy_create, NULL);

	/*
	 * Set up LDT. We use gldt to set up the syscall and sigret
	 * call gates, and ldt/ldta for the code/data segments.
	 */
	gldt = (struct gate_desc *)u.u_procp->p_ldt;
	gldt[seltoi(USER_SCALL)] = scall_dscr;
	gldt[seltoi(USER_SIGCALL)] = sigret_dscr;

	ldt = (struct dscr *)u.u_procp->p_ldt;
	ldt += seltoi(USER_CS);
	ldt->a_base0015 = 0;
	ldt->a_base1623 = 0;
	ldt->a_base2431 = 0;
	ldt->a_lim0015 = (ushort)btoct(MAXUVADR-1);
	ldt->a_lim1619 = ((unsigned char)(btoct(MAXUVADR-1) >> 16)) & 0x0F;
	ldt->a_acc0007 = UTEXT_ACC1;
	ldt->a_acc0811 = TEXT_ACC2;

	ldta = (struct dscr *)u.u_procp->p_ldt;
	ldta += seltoi(USER_DS);
	*ldta = *ldt;
	ldta->a_acc0007 = UDATA_ACC1;

#ifdef WEITEK
	ldta->a_lim0015 = (ushort)btoct(WEITEK_MAXADDR);
	ldta->a_lim1619 = ((unsigned char)(btoct(WEITEK_MAXADDR) >> 16)) & 0x0F;
#endif

	/*
	 * Set up LDT entries for floating point emulation.
	 * 2 entries: one for a 32-bit alias to the user's stack,
	 *   and one for a window into the fp save area in the
	 *   user structure.
	 */
	ldt = (struct dscr *)u.u_procp->p_ldt;
	ldt += seltoi(USER_FP);

	setdscrbase(ldt, &u.u_fps);
	i = sizeof(u.u_fps);

#ifdef WEITEK
	i += sizeof(u.u_weitek_reg);
#endif
	setdscrlim(ldt, i);
	ldt->a_acc0007 = UDATA_ACC1;
	ldt->a_acc0811 = DATA_ACC2_S;

	ldt = (struct dscr *)u.u_procp->p_ldt;
	ldt += seltoi(USER_FPSTK);
	*ldt = *ldta;
}
#endif

/*
 * Machine-independent initialization code.
 *
 * We are called from start.s with all interrupts disabled. They are
 * enabled in startup(), which is in machine-dependent code.
 *
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
	proc_t *p;

	startup();
	clkstart();
	cred_init();
	dnlc_init();
	inituname();

	prt_where = PRW_CONS;

	/*
	 * Lets display the banner early so the user has some idea that
	 * Unix is taking over the system.
	 *
	 * Good {morning, afternoon, evening, night}.
	 */
	cmn_err(CE_CONT, "^\n");	/* Need a newline for alternate console */
	cmn_err(CE_CONT,
		"JadeOS Release %s Version %s [UNIX(R) System V Release 4.0]\n",
		utsname.release, utsname.version);
	cmn_err(CE_CONT, "%s\n", copyright);
	cmn_err(CE_CONT, "All rights reserved.\n");

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

	spl0();

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

		/*
		 * The function below is for x86 only.
		 */
#ifdef __i386__
		init_finish(p);
#endif

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
