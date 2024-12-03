/* 
 * Mach Operating System
 * Copyright (c) 1993-1987 Carnegie Mellon University
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
 *	File:	kernel/mach/machine_main.c
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1987
 *
 *	Support for machine independent machine abstraction.
 */

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/machine.h>
#include <mach/host_info.h>
#include <mach/kern/assert.h>
#include <mach/kern/counters.h>
#include <mach/kern/ipc_host.h>
#include <mach/kern/host.h>
#include <mach/kern/lock.h>
#include <mach/kern/machine.h>
#include <mach/kern/processor.h>
#include <mach/kern/queue.h>
#include <mach/kern/strings.h>
#include <mach/kern/task.h>
#include <mach/kern/thread.h>
#include <machine/machspl.h>	/* for splsched */
#include <sys/reboot.h>

/*
 *	cpu_up:
 *
 *	Flag specified cpu as up and running.  Called when a processor comes
 *	online.
 */
void
cpu_up(int cpu)
{
	processor_t processor;
	spl_t s;

	processor = cpu_to_processor(cpu);
	pset_lock(&default_pset);
	s = splsched();
	processor_lock(processor);
#if	NCPUS > 1
	init_ast_check(processor);
#endif	/* NCPUS > 1 */
	machine_slot[cpu].running = TRUE;
	machine_info.avail_cpus++;
	pset_add_processor(&default_pset, processor);
	processor->state = PROCESSOR_RUNNING;
	processor_unlock(processor);
	splx(s);
	pset_unlock(&default_pset);
}

/*
 *	cpu_down:
 *
 *	Flag specified cpu as down.  Called when a processor is about to
 *	go offline.
 */
void
cpu_down(int cpu)
{
	processor_t processor;
	spl_t s;

	s = splsched();
	processor = cpu_to_processor(cpu);
	processor_lock(processor);
	machine_slot[cpu].running = FALSE;
	machine_info.avail_cpus--;

	/*
	 *	Processor has already been removed from pset.
	 */
	processor->processor_set_next = PROCESSOR_SET_NULL;
	processor->state = PROCESSOR_OFF_LINE;
	processor_unlock(processor);
	splx(s);
}

/*
 *	[ exported ]
 *	host_reboot:
 *
 *	Reboot or halt the system,
 *	or trap into the kernel debugger (for user-level panics).
 */
kern_return_t
host_reboot(host_t host, int options)
{
	if (host == HOST_NULL)
		return KERN_INVALID_HOST;

	if (options & RB_DEBUGGER)
		Debugger("Debugger");
	else
		halt_all_cpus(!(options & RB_HALT));

	return KERN_SUCCESS;
}
