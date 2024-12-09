/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

/*
 * kernel/arch/i386/i386_startup.c
 *
 * Copyright (c) 2024 Stefanos Stefanidis.
 * All rights reserved.
 */

/*
 * Machine-dependent startup code. Called by main() to initialize
 * various data structures. Returns back to main() once finished.
 *
 * TODO: Figure out if we are going to continue supporting EISA in
 * future releases.
 */
void
startup(void)
{
	int	i, binfosum;

	/*
	 * Checksum the bootinfo structure.
	 */
	binfosum = 0;
	for (i = 0; i < (sizeof(struct bootinfo) - sizeof(int)); i++)
		binfosum += ((char *)&bootinfo)[i];

	if (binfosum == bootinfo.checksum)
		checksumOK = 1;

	/*
 	 * Retrieve information passed from boot via the bootinfo struct.
 	 */
	bootarg_parse();

	/*
	 * Check to see if this is an EISA machine.
	 */
	eisa_enable = inb(EISA_CFG3) != 0;

	/*
	 * Time to store the ROM font locations we got from the bios
	 * in uprt.s into the driver's font pointers.
	 */
	for (i = 0; i < 5; i++)
		egafont_p[i] = (unchar *)phystokv(ftop(egafontptr[i]));

	/*
	 * On an AT386 machine, this is the size (in K) of "base" memory.
	 */
	bmemsize = bootinfo.memavail[0].extent >> 10;

	/*
	 * Make sure memavail[] memory chunks are page-aligned.
	 */
	for (i = 0; i < bootinfo.memavailcnt; i++) {
		bootinfo.memavail[i].extent = ctob(btoc(bootinfo.memavail[i].extent));
		bootinfo.memavail[i].base = ctob(btoct(bootinfo.memavail[i].base));
	}

	/*
	 * Make sure memused[] memory chunks are page-aligned.
	 */
	for (i = 0; i < bootinfo.memusedcnt; i++) {
		bootinfo.memused[i].extent = ctob(btoc(bootinfo.memused[i].extent));
		bootinfo.memused[i].base = ctob(btoct(bootinfo.memused[i].base));
	}

	/*
	 * Handle relocation of kernel text in special memory, if possible.
	 * Revises bootinfo.memused[] based on kernel text relocation.
	 */
	ml_reloc_ktext();

	/*
	 * Recalculate checksum in binfo, now that it has changed.
	 */
	binfosum = 0;
	for ( i = 0; i < (sizeof(struct bootinfo) - sizeof(int)); i++ )
		binfosum += ((char *)&bootinfo)[i];
	bootinfo.checksum = binfosum;

	/*
	 * Determine which portions of memavail[] are NOT already in
	 * memused[].  At this point, memused[] contains the tuples
	 * for memory allocated for kernel text and data, but not for bss.
	 */
	ml_unused_mem();

	free_paddr = memNOTused[0].base; /* first free physical addr */

	/*
	 * Allocate space for the BSS and set up kernel address page table.
	 */
	bss_size = btoc(&end) - btoct(sbss);	/* size of bss (clicks) */
	pt = kspt0 + btoct(sbss - KVSBASE);	/* ptr to bss ptes in kspt0[] */

	for (i = memsize = 0; bss_size-- > 0;) {
		if (i >= memNOTusedcnt)
			cmn_err(CE_PANIC, "Not enough physical memory to boot.\n");
		if (free_paddr >= memNOTused[i].base + memNOTused[i].extent) {
			/* crossing unused mem boundary in memNOTused[] */
			memsize += memNOTused[i].extent;
			++i;
			free_paddr = memNOTused[i].base;
		}
		(pt++)->pg_pte = mkpte(PG_V, PFNUM(free_paddr));
		free_paddr += NBPC;
	}

	/*
	 * Save index into memNOTused[], so we know which memNOTused[]
	 * entries are (by now) actually used.
	 */
	memused_idx = i;

	if (free_paddr >= memNOTused[memused_idx].base
				+ memNOTused[memused_idx].extent) {
		/*
		 * Current free_paddr is crossing unused mem boundary in 
		 * memNOTused[].  Make free_paddr point to the next actual
		 * unused physical address.
		 */
		memused_idx++;
		free_paddr = memNOTused[memused_idx].base;
	}

	flushtlb();

	/*
	 * Check maxpmem tuneable.
	 */
	if (v.v_maxpmem) {
		unsigned maxpmem, part_used, clipped, extent;

		clipped = 0;

		/* 
		 * Don't let maxpmem be less than the size of physical memory
		 * (kernel text + data + bss + boot stuff) already used.
		 */

		/* core_size <= sizeof bss */
		part_used = free_paddr - memNOTused[memused_idx].base;
		core_size = memsize + part_used;

		/* add in rest of used memory */
		for (i = 0; i < bootinfo.memusedcnt; i++) {
			core_size += bootinfo.memused[i].extent;		
		}

		if (v.v_maxpmem < btoc(core_size))
			v.v_maxpmem = btoc(core_size);
		maxpmem = ctob(v.v_maxpmem);
		extent = memNOTused[memused_idx].base
			+ memNOTused[memused_idx].extent - free_paddr;
		for (i = memused_idx; i < memNOTusedcnt; ) {
			if (extent > (maxpmem - core_size)) {
				memNOTused[i].extent = part_used + maxpmem - core_size;
				clipped++;
			}
			core_size += memNOTused[i].extent - part_used;
			if (++i >= memNOTusedcnt)
				break;
			part_used = 0;
			extent = memNOTused[i].extent;
			
		}

		/*
		 * Update bootinfo.memavail[] if any memNOTused[] entries were
		 * clipped.
		 */
		if (clipped)
			ml_update_avail();	
	}

	/*
	 * Zero all BSS. Can't use bzero since the user block isn't mapped.
	 */
	wzero(sbss, (&end - sbss + 3) & ~3);

	/*
	 * Set up memory parameters.
	 */
	nextclick = firstfree = btoct(free_paddr);
	physmem = 0;

	/* 
	 * Set maxclick according to the bootinfo memavail[].
	 */
	maxclick = btoct(bootinfo.memavail[bootinfo.memavailcnt - 1].base
		+ bootinfo.memavail[bootinfo.memavailcnt - 1].extent);

	/*
	 * Unfortunately, for AT386 we have to support device drivers
	 * which assume the device memory at bmemsize to 1M is always
	 * mapped at KVBASE + bmemsize.
	 */
	nextclick = scanmem(nextclick, btoct(bmemsize*1024), btoct(1024*1024));
	physmem = 0;	/* Don't count device memory in physmem */

	/*
	 * Set up linear map for all real memory.
	 */
	for (i = 0; i < bootinfo.memavailcnt && bootinfo.memavail[i].extent;++i) {
		firstclick = btoct(bootinfo.memavail[i].base);
		lastclick = btoct(bootinfo.memavail[i].base + bootinfo.memavail[i].extent);
		nextclick = scanmem(nextclick, firstclick, lastclick);
	}

	/*
	 * After we have come up we no longer need addresses at linear 0.
	 * Make them illegal.  These addresses were needed during the
	 * initial code in uprt.s as linear must equal physical for the
	 * <cs,pc> prior to the jmp.  Setting linear addresses at 0 to
	 * invalid breaks pmon.
	 */
	kpd0[0].pg_pte = mkpte(0, 0);

	flushtlb();

	wzero(phystokv(ctob(nextclick)),
		ctob(btoct(memNOTused[memused_idx].base +
		memNOTused[memused_idx].extent) - nextclick));

	for (i = memused_idx+1; i < memNOTusedcnt; i++)
		wzero(phystokv(memNOTused[i].base), memNOTused[i].extent);

	/*
	 * Allocate user block for proc 0.
	 */
	nextclick = p0u(nextclick);

	/*
	 * Initialize memory mapping for sysseg. The segment from which the
	 * kernel dynamically allocates space for itself.
	 */
	nextclick = sysseginit(nextclick);

	/*
	 * Initialize the pio window segments.
	 */
	nextclick = pioseginit(nextclick);

	/*
	 * Initialize the map used to allocate pio virtual space.
	 */
	mapinit(piomap, piomapsz);
	mfree(piomap, piosegsz, btoc(piosegs));

	/*
	 * Allocate page table for kvsegmap.
	 */

	/*
	 * Initialize kas stuff. At this point kvseg is initialized, hence
	 * calls to sptalloc now becomes valid - henceforth.
	 */
	nextclick = kvm_init(nextclick);

	/*
	 * Do scheduler initialization.
	 */
	dispinit();

	/*
	 * Initialize the high-resolution timer free list.
	 */
	hrtinit();

	/*
	 * Initialize the interval timer free list.
	 */
	itinit();

	/*
	 * Initialize process 0 (swapper).
	 */
	p0init();

	/*
	 * Initialize process table.
	 */
	pid_init();

	/*
	 * 16 MiB DMA - Allow the x86 to access more than 16 MiB of memory.
	 */
	if (dma_check_on) {
		set_dmalimits();
		reserve_dma_pages();
	}

	/*
	 * Configure the system.
	 */
	configure();

	/*
	 * Now finish the 80386 B1 stepping (errata #21) workaround.
	 */
	u.u_tss->t_cr3 |= fp387cr3;
	ktss.t_cr3 |= fp387cr3;
	dftss.t_cr3 |= fp387cr3;

	/*
	 * Set the eisa_bus variable before driver inits.
	 */
	if ((bootinfo.id[0] == 'I') && (bootinfo.id[1] == 'D') &&
		(bootinfo.id[2] == 'N') && (bootinfo.id[3] == 'O'))
		id = bootinfo.id[4];

	if ((id == C2) || (id == C3) || (id == C4))
		eisa_bus = 0;
	else {
		outb(eisa_port, portval);
		portval = inb(eisa_port);
		if (portval == (char)0xFF)
			eisa_bus = 0;
		else {
			eisa_bus = 1;
			byte = inb(eisa_id_port);	/* grab 0xc82 */
			eisa_brd_id = byte;
			byte = inb(eisa_id_port + 1);	/* get 0xc83 */
			eisa_brd_id += byte << 8;
		}
	}

	cmn_err(CE_CONT, "%s bus\n\n", eisa_enable ? "EISA" : "AT386");

	picinit();      /* initialize PICs, but do not enable interrupts */

	/*
	 * General hook for things which need to run prior to turning on
	 * interrupts.
	 */
	oem_pre_clk_init();

	/*
	 * Was the bootinfo structure OK when recieved from boot?
	 */
	if (checksumOK != 1)
		cmn_err(CE_WARN, "Bootinfo checksum incorrect.\n\n");

	/*
	 * Assert: (kernel stack + floating point stuff) is equal to 1 page.
	 */
	if (((char *)&u + NBPP) != (char *) &u.u_tss)
		cmn_err(CE_PANIC, "startup: Invalid Kernel Stack Size in U block\n");

	if (dma_check_on)
		setup_dma_strategies();
}
