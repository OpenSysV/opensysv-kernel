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

/*
 * kernel/riscv/riscv_startup.c
 *
 * This file contains the startup code for the RISC-V architecture.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>

#include <riscv/riscv.h>
#include <riscv/memlayout.h>

STATIC void startup_finish(void);
STATIC void devinit(void);

volatile STATIC int started = 0;
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

/*
 * Machine-dependent startup code.
 */
void
startup(void)
{
	/*
	 * Set M Previous Privilege mode to Supervisor, for mret.
	 */
	unsigned long x = r_mstatus();
	x &= ~MSTATUS_MPP_MASK;
	x |= MSTATUS_MPP_S;
	w_mstatus(x);

	/*
	 * Set M Exception Program Counter to main, for mret.
	 */
	w_mepc((uint64)startup_finish);

	/*
	 * Disable paging for now.
	 */
	w_satp(0);

	/*
	 * Delegate all interrupts and exceptions to supervisor mode.
	 */
	w_medeleg(0xffff);
	w_mideleg(0xffff);
	w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

	/*
	 * Configure Physical Memory Protection to give supervisor mode
	 * access to all of physical memory.
	 */
	w_pmpaddr0(0x3fffffffffffffull);
	w_pmpcfg0(0xf);

	/*
	 * Ask for clock interrupts.
	 */
	timerinit();

	/*
	 * Keep each CPU's hartid in its tp register, for cpuid().
	 */
	int id = r_mhartid();
	w_tp(id);

	/*
	 * Switch to supervisor mode and jump to startup_finish().
	 */
	asm volatile("mret");
}

STATIC void
startup_finish(void)
{
	if (cpuid() == 0) {
		kinit();
		kvminit();
		kvminithart();
		trapinit();
		trapinithart();
		plicinit();
		plicinithart();
		__sync_synchronize();
		started = 1;
	} else {
		while(started == 0)
			;
		__sync_synchronize();
		kvminithart();
		trapinithart();
		plicinithart();
	}

	devinit();
}

/*
 * Function:
 * devinit()
 *
 * Description:
 * Initialize all installed system devices, like SD card readers,
 * hard drives, SSDs, CD/DVD drives, etc.
 *
 * Arguments:
 * None.
 *
 * Return value:
 * None.
 */
STATIC void
devinit(void)
{
	cmn_err(CE_CONT, "Root on %lu (%d/%d)\n", rootdev, major(rootdev),
		minor(rootdev));
}
