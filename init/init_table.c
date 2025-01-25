/*	Copyright (c) 1990, 1991, 1992 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 2024, 2025 Stefanos Stefanidis.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/clist.h>
#include <sys/conf.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/var.h>

/*
 * Array containing the addresses of the various initializing
 * routines executed by main() at boot time.
 */
int (*init_tbl[])() = {
	cinit,
	(int(*)())binit,
	vfsinit,
	(int(*)())finit,
	seminit,
	msginit,
	strinit,
	0
};
