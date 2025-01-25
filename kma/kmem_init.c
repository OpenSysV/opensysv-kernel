/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/tuneable.h>
#include <sys/cmn_err.h>

/*
 * Function:
 * kmem_init()
 *
 * Description:
 * Initialize memory manager. Run from main() at boot time.
 *
 * Call kmem_alloc[sb]pool to allocate 1 pool of each size small pool
 * *MUST* be allocated before big, because will call kmem_alloc() to get
 * memory for structs and bitmap for big pool.
 *
 * Allocate a "golden" buffer from each pool to prevent thrashing under
 * light load.
 */
void
kmem_init(void)
{
	if (kmem_allocspool(KM_NOSLEEP) != SUCCESS)
		cmn_err(CE_PANIC, "kmem_init: failed to allocate small pool\n");

	Km_Golden[0] = (unchar *)kmem_alloc(MINASMALL, KM_NOSLEEP);

	if (kmem_allocbpool(KM_NOSLEEP) != SUCCESS)
		cmn_err(CE_PANIC, "kmem_init: failed to allocate big pool\n");

	Km_Golden[1] = (unchar *)kmem_alloc(MINABIG, KM_NOSLEEP);
	return;
}
