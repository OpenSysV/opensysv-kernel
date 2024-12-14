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
 *	File:	kernel/mach/mach_zalloc.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Zone-based memory allocator.  A zone is a collection of fixed size
 *	data blocks for which quick allocation/deallocation is possible.
 */

#include <mach/assert.h>
#include <mach/macro_help.h>
#include <mach/memory.h>
#include <mach/sched.h>			/* sched_tick */
#include <mach/sched_prim.h>
#include <mach/strings.h>
#include <mach/zalloc.h>
#include <mach/vm_param.h>
#include <vm/vm_kern.h>
#include <machine/machspl.h>
#include <sys/cmn_err.h>

/*
 * Function:
 * zinit()
 *
 * Description:
 * zinit() initializes a new zone. The zone data structures themselves
 * are stored in a zone, which is initially a static structure that is
 * initialized by zone_init().
 *
 * Arguments:
 * size: The size of an element.
 * max: Maximum memory to use.
 * alloc: Allocation size.
 * pageable: Is this zone pageable or not?
 * name: A name for the zone.
 *
 * Return value:
 * The zone structure.
 */
zone_t
zinit(vm_size_t	size, vm_size_t max, vm_size_t alloc, boolean_t pageable,
	char *name)
{
	zone_t z;

	if (zone_zone == ZONE_NULL)
		z = (zone_t) zget_space(sizeof(struct zone));
	else
		z = (zone_t) zalloc(zone_zone);

	if (z == ZONE_NULL)
		cmn_err(CE_PANIC, "zinit: Cannot allocate memory for zone");

 	if (alloc == 0)
		alloc = PAGE_SIZE;

	if (size == 0)
		size = sizeof(z->free_elements);

	/*
	 *	Round off all the parameters appropriately.
	 */
	if ((max = round_page(max)) < (alloc = round_page(alloc)))
		max = alloc;

	z->free_elements = 0;
	z->cur_size = 0;
	z->max_size = max;
	z->elem_size = ((size-1) + sizeof(z->free_elements)) -
			((size-1) % sizeof(z->free_elements));

	z->alloc_size = alloc;
	z->pageable = pageable;
	z->zone_name = name;
	z->count = 0;
	z->doing_alloc = FALSE;
	z->exhaustible = z->sleepable = FALSE;
	z->collectable = FALSE;
	z->expandable  = TRUE;
	lock_zone_init(z);

	/*
	 *	Add the zone to the all-zones list.
	 */
	z->next_zone = ZONE_NULL;
	simple_lock(&all_zones_lock);
	*last_zone = z;
	last_zone = &z->next_zone;
	num_zones++;
	simple_unlock(&all_zones_lock);

	return z;
}

/*
 *	Cram the given memory into the specified zone.
 */
void
zcram(zone_t zone, vm_offset_t newmem, vm_size_t size)
{
	vm_size_t elem_size;

	if (newmem == (vm_offset_t) 0)
		cmn_err(CE_PANIC, "zcram: memory at zero");

	elem_size = zone->elem_size;

	lock_zone(zone);
	while (size >= elem_size) {
		ADD_TO_ZONE(zone, newmem);
		zone_page_alloc(newmem, elem_size);
		zone->count++;	/* compensate for ADD_TO_ZONE */
		size -= elem_size;
		newmem += elem_size;
		zone->cur_size += elem_size;
	}
	unlock_zone(zone);
}

/*
 * Contiguous space allocator for non-paged zones. Allocates "size" amount
 * of memory from zone_map.
 */
vm_offset_t
zget_space(vm_offset_t size)
{
	vm_offset_t new_space = 0;
	vm_offset_t result;
	vm_size_t space_to_add = 0; /*'=0' to quiet gcc warnings */

	simple_lock(&zget_space_lock);
	while ((zalloc_next_space + size) > zalloc_end_of_space) {
		/*
		 *	Add at least one page to allocation area.
		 */

		space_to_add = round_page(size);

		if (new_space == 0) {
			/*
			 *	Memory cannot be wired down while holding
			 *	any locks that the pageout daemon might
			 *	need to free up pages.  [Making the zget_space
			 *	lock a complex lock does not help in this
			 *	regard.]
			 *
			 *	Unlock and allocate memory.  Because several
			 *	threads might try to do this at once, don't
			 *	use the memory before checking for available
			 *	space again.
			 */
			simple_unlock(&zget_space_lock);

			if (kmem_alloc_wired(zone_map, &new_space, space_to_add)
				!= KERN_SUCCESS)
				return 0;
			zone_page_init(new_space, space_to_add, ZONE_PAGE_USED);
			simple_lock(&zget_space_lock);
			continue;
		}

		
		/*
	  	 *	Memory was allocated in a previous iteration.
		 *
		 *	Check whether the new region is contiguous
		 *	with the old one.
		 */
		if (new_space != zalloc_end_of_space) {
			/*
			 *	Throw away the remainder of the
			 *	old space, and start a new one.
			 */
			zalloc_wasted_space +=
				zalloc_end_of_space - zalloc_next_space;
			zalloc_next_space = new_space;
		}

		zalloc_end_of_space = new_space + space_to_add;

		new_space = 0;
	}

	result = zalloc_next_space;
	zalloc_next_space += size;		
	simple_unlock(&zget_space_lock);

	if (new_space != 0)
		kmem_free(zone_map, new_space, space_to_add);

	return result;
}

/*
 *	Initialize the "zone of zones" which uses fixed memory allocated
 *	earlier in memory initialization.  zone_bootstrap is called
 *	before zone_init.
 */
void
zone_bootstrap(void)
{
	simple_lock_init(&all_zones_lock);
	first_zone = ZONE_NULL;
	last_zone = &first_zone;
	num_zones = 0;

	simple_lock_init(&zget_space_lock);
	zalloc_next_space = zdata;
	zalloc_end_of_space = zdata + zdata_size;
	zalloc_wasted_space = 0;

	zone_zone = ZONE_NULL;
	zone_zone = zinit(sizeof(struct zone), 128 * sizeof(struct zone),
		sizeof(struct zone), FALSE, "zones");
}

void
zone_init(void)
{
	vm_offset_t	zone_min;
	vm_offset_t	zone_max;
	vm_size_t zone_table_size;

	zone_map = kmem_suballoc(kernel_map, &zone_min, &zone_max,
		zone_map_size, FALSE);

	/*
	 * Setup garbage collection information:
	 */
 	zone_table_size = atop(zone_max - zone_min) * 
		sizeof(struct zone_page_table_entry);
	if (kmem_alloc_wired(zone_map, (vm_offset_t *) &zone_page_table,
		zone_table_size) != KERN_SUCCESS)
		cmn_err(CE_PANIC, "zone_init: Failed to initialize zones.");

	zone_min = (vm_offset_t)zone_page_table + round_page(zone_table_size);
	zone_pages = atop(zone_max - zone_min);
	zone_map_min_address = zone_min;
	zone_map_max_address = zone_max;
	simple_lock_init(&zone_page_table_lock);
	zone_page_init(zone_min, zone_max - zone_min, ZONE_PAGE_UNUSED);
}

/*
 *	zalloc returns an element from the specified zone.
 */
vm_offset_t
zalloc(zone_t zone)
{
	vm_offset_t	addr;

	if (zone == ZONE_NULL)
		cmn_err(CE_PANIC, "zalloc: Attempted to access null zone");

	check_simple_locks();

	lock_zone(zone);
	REMOVE_FROM_ZONE(zone, addr, vm_offset_t);
	while (addr == 0) {
		/*
 		 *	If nothing was there, try to get more
		 */
		if (zone->doing_alloc) {
			/*
			 *	Someone is allocating memory for this zone.
			 *	Wait for it to show up, then try again.
			 */
			assert_wait((event_t)&zone->doing_alloc, TRUE);
			/* XXX say wakeup needed */
			unlock_zone(zone);
			thread_block(CONTINUE_NULL);
			lock_zone(zone);
		}
		else {
			if ((zone->cur_size + (zone->pageable ?
				zone->alloc_size : zone->elem_size)) >
			    zone->max_size) {
				if (zone->exhaustible)
					break;
				/*
				 * Printf calls logwakeup, which calls
				 * select_wakeup which will do a zfree
				 * (which tries to take the select_zone
				 * lock... Hang.  Release the lock now
				 * so it can be taken again later.
				 * NOTE: this used to be specific to
				 * the select_zone, but for
				 * cleanliness, we just unlock all
				 * zones before this.
				 */
				if (zone->expandable) {
					/*
					 * We're willing to overflow certain
					 * zones, but not without complaining.
					 *
					 * This is best used in conjunction
					 * with the collecatable flag. What we
					 * want is an assurance we can get the
					 * memory back, assuming there's no
					 * leak. 
					 */
					zone->max_size += (zone->max_size >> 1);
				} else if (!zone_ignore_overflow) {
					unlock_zone(zone);
					cmn_err(CE_PANIC, "zalloc: zone \"%s\" empty.\n",
						zone->zone_name);
				}
			}

			if (zone->pageable)
				zone->doing_alloc = TRUE;
			unlock_zone(zone);

			if (zone->pageable) {
				if (kmem_alloc_pageable(zone_map, &addr, zone->alloc_size)
					!= KERN_SUCCESS)
					cmn_err(CE_PANIC, "zalloc: Cannot allocate pageable zone.");
				zcram(zone, addr, zone->alloc_size);
				lock_zone(zone);
				zone->doing_alloc = FALSE; 
				/* XXX check before doing this */
				thread_wakeup((event_t)&zone->doing_alloc);

				REMOVE_FROM_ZONE(zone, addr, vm_offset_t);
			} else  if (zone->collectable) {
				if (kmem_alloc_wired(zone_map, &addr, zone->alloc_size)
							!= KERN_SUCCESS)
					cmn_err(CE_PANIC, "zalloc: Cannot allocate collectable zone.");
				zone_page_init(addr, zone->alloc_size,
							ZONE_PAGE_USED);
				zcram(zone, addr, zone->alloc_size);
				lock_zone(zone);
				REMOVE_FROM_ZONE(zone, addr, vm_offset_t);
			} else {
				addr = zget_space(zone->elem_size);
				if (addr == 0)
					cmn_err(CE_PANIC, "zalloc: zget_space() returned 0");

				lock_zone(zone);
				zone->count++;
				zone->cur_size += zone->elem_size;
				unlock_zone(zone);
				zone_page_alloc(addr, zone->elem_size);
				return addr;
			}
		}
	}

	unlock_zone(zone);
	return addr;
}

/*
 *	zget returns an element from the specified zone
 *	and immediately returns nothing if there is nothing there.
 *
 *	This form should be used when you can not block (like when
 *	processing an interrupt).
 */
vm_offset_t
zget(zone_t zone)
{
	vm_offset_t addr;

	if (zone == ZONE_NULL)
		cmn_err(CE_PANIC, "zalloc: null zone");

	lock_zone(zone);
	REMOVE_FROM_ZONE(zone, addr, vm_offset_t);
	unlock_zone(zone);

	return addr;
}

boolean_t zone_check = FALSE;

void
zfree(zone_t zone, vm_offset_t elem)
{
	lock_zone(zone);
	if (zone_check) {
		vm_offset_t this;

		/*
		 * Check the zone's consistency.
		 */
		for (this = zone->free_elements; this != 0;
			this = * (vm_offset_t *) this)
			if (this == elem)
				cmn_err(CE_PANIC, "zfree: Zone consistency check failed");
	}
	ADD_TO_ZONE(zone, elem);
	unlock_zone(zone);
}

void
zcollectable(zone_t zone)
{
	zone->collectable = TRUE;
}

void
zchange(zone_t zone, boolean_t pageable, boolean_t sleepable,
	boolean_t exhaustible, boolean_t collectable)
{
	zone->pageable = pageable;
	zone->sleepable = sleepable;
	zone->exhaustible = exhaustible;
	zone->collectable = collectable;
	lock_zone_init(zone);
}
