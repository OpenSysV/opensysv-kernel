/*
 * kernel/riscv/start.s
 *
 * Minimal RISC-V assembly language bootstrap to enable calling
 * main() in C. This is the first kernel code executed.
 */

#include <riscv/riscv.h>
#include <riscv/memlayout.h>

_start:

/*
 * Halt processor. Effictively we are waiting for an interrupt that
 * will never come.
 */
haltsys:
	wfi
	jmp -1(pc)	/* if we break out, continue waiting. */
