/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

#include <mach/kern_return.h>
#include <mach/time_stamp.h>

unsigned ts_tick_count;

/*
 * Initialization procedure.
 */
void
timestamp_init(void)
{
	ts_tick_count = 0;
}

kern_return_t
kern_timestamp(struct tsval *tsp)
{
	struct tsval temp;

	temp.low_val = 0;
	temp.high_val = ts_tick_count;

	if (copyout(&temp, tsp, sizeof(struct tsval)) != KERN_SUCCESS)
		return(KERN_INVALID_ADDRESS);
	return(KERN_SUCCESS);
}
