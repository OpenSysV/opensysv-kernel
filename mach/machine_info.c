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
 *	File:	kernel/mach/machine_info.c
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1987
 *
 *	Machine information routines (exported).
 */

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/machine.h>
#include <mach/kern/host.h>
#include <mach/kern/task.h>

/*
 *	Exported variables:
 */

struct machine_info	machine_info;
struct machine_slot	machine_slot[NCPUS];

/*
 *	[ obsolete, exported ]
 *	xxx_host_info:
 *
 *	Return the host_info structure.
 */
kern_return_t
xxx_host_info(task_t task, machine_info_t info)
{
	*info = machine_info;
	return KERN_SUCCESS;
}

/*
 *	[ obsolete, exported ]
 *	xxx_slot_info:
 *
 *	Return the slot_info structure for the specified slot.
 */
kern_return_t
xxx_slot_info(task_t task, int slot, machine_slot_t info)
{
	if (slot < 0 || slot >= NCPUS)
		return KERN_INVALID_ARGUMENT;
	*info = machine_slot[slot];
	return KERN_SUCCESS;
}

/*
 *	[ obsolete, exported, not implemented ]
 *	xxx_cpu_control:
 *
 *	Support for user control of cpus.  The user indicates which cpu
 *	he is interested in, and whether or not that cpu should be running.
 */
kern_return_t
xxx_cpu_control(task_t task, int cpu, boolean_t runnable)
{
	return KERN_FAILURE;
}

/*
 *	[ exported ]
 *
 *	Returns the boot environment string provided
 *	by the bootstrap loader, if there is one.
 */
kern_return_t
host_get_boot_info(host_t priv_host, kernel_boot_info_t boot_info)
{
	char *src = "";

	if (priv_host == HOST_NULL)
		return KERN_INVALID_HOST;

	(void) strncpy(boot_info, src, KERNEL_BOOT_INFO_MAX);
	return KERN_SUCCESS;
}
