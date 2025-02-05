/*
 * Copyright (c) 2024, 2025 Stefanos Stefanidis.
 * All rights reserved.
 */

/*
 * kernel/init/init_standsh.c
 * Derived from ken/uproc.c from V6on286.
 *
 * Copyright (c) 1992 Szigeti Szabolcs.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/statvfs.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>

STATIC void standsh_write(void);
STATIC void standsh_help(void);
STATIC void print_sysinfo(void);

void
start_standsh(void)
{
	cmn_err(CE_CONT, "OpenSysV Standalone Shell\n");
	cmn_err(CE_CONT, "Type 'help' for a list of built-in commands.\n");
}

STATIC void
print_help(void)
{
	cmn_err(CE_CONT,
		"Supported commands are: \n\n"
		"cd:\tChange directory.\n"
		"exec:\tExecute binary program.\n"
		"ls:\tList directory.\n"
		"mount:\tMount a file system.\n"
		"sync:\tSync mounted file systems.\n"
		"sysinfo:\tPrint system information.\n"
		"umount:\tUnmount a file system.\n"
	);
}

STATIC void
print_sysinfo(void)
{
	cmn_err(CE_CONT, "OpenSysV %s\n", version);
	cmn_err(CE_CONT, "Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T.\n");
	cmn_err(CE_CONT, "Copyright (c) 1990 UNIX System Laboratories, Inc.\n");
	cmn_err(CE_CONT, "Copyright (c) 1987-1993 Carnegie-Mellon University.\n");
	cmn_err(CE_CONT, "Copyright (c) 2024, 2025 The OpenSysV Project.\n");
	cmn_err(CE_CONT, "All rights reserved.\n");
}

/*
 * Write to a file.
 */
STATIC void
standsh_write(void)
{
	cmn_err(CE_CONT, "standsh: Writing to files is not implemented!\n");
}
