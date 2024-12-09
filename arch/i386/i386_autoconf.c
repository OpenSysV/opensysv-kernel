/*
 * kernel/arch/i386/i386_autoconf.c
 *
 * Copyright (c) 2024 Stefanos Stefanidis.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>

/*
 * configure() - Configure system hardware.
 *
 * Determine mass storage and memory configuration for a machine.
 * Get cpu type, and then switch out to machine specific procedures
 * which will probe adaptors to see what is out there.
 */
void
configure(void)
{
	/*
	 * Fetch CPUID from processor. This instruction is only supported
	 * on an Intel 486SL and newer processors.
	 */
	if (is_cpuid_available)
		get_cpuid();
	else
		cmn_err(CE_WARN, "Instruction 0x0FA2 not available.\n");

	/*
	 * Pull in the floating point emulator.
	 */
	fpeinit();
}
