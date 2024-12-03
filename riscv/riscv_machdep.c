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
#include <sys/systm.h>
#include <sys/cmn_err.h>

STATIC void devinit(void);

/*
 * Machine-dependent startup code.
 */
void
startup(void)
{
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
