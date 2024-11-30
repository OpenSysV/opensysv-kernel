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
 *	File:	mach/thread/thread_info.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young, David Golub
 *	Date:	1986
 *
 *	Thread management primitives implementation.
 */

/*
 *	Return thread's machine-dependent state.
 */
kern_return_t
thread_get_state(
	register thread_t	thread,
	int			flavor,
	thread_state_t		old_state,	/* pointer to OUT array */
	natural_t		*old_state_count)	/*IN/OUT*/
{
	kern_return_t		ret;

	if (thread == THREAD_NULL || thread == current_thread()) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_hold(thread);
	(void) thread_dowait(thread, TRUE);

	ret = thread_getstatus(thread, flavor, old_state, old_state_count);

	thread_release(thread);
	return ret;
}

/*
 *	Change thread's machine-dependent state.
 */
kern_return_t
thread_set_state(thread_t thread, int flavor, thread_state_t new_state,
	natural_t new_state_count)
{
	kern_return_t ret;

	if (thread == THREAD_NULL || thread == current_thread()) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_hold(thread);
	(void) thread_dowait(thread, TRUE);

	ret = thread_setstatus(thread, flavor, new_state, new_state_count);

	thread_release(thread);
	return ret;
}

/*
 * Return information about the specified thread.
 *
 * thread_info_out: pointer to OUT array
 * thread_info_count: IN/OUT
 */
kern_return_t
thread_info(thread_t thread, int flavor, thread_info_t thread_info_out,
	natural_t *thread_info_count)
{
	int state, flags;
	spl_t s;

	if (thread == THREAD_NULL)
		return KERN_INVALID_ARGUMENT;

	if (flavor == THREAD_BASIC_INFO) {
	    thread_basic_info_t basic_info;
	    unsigned int sleep_time;
	    time_spec_t user_time, system_time;

	    if (*thread_info_count < THREAD_BASIC_INFO_COUNT) {
		return KERN_INVALID_ARGUMENT;
	    }

	    basic_info = (thread_basic_info_t) thread_info_out;

	    s = splsched();
	    thread_sched_lock(thread);

	    /*
	     *	Update lazy-evaluated scheduler info because someone wants it.
	     *	Grab sleep-time first, since UPDATE_PRIORITY zeros it.
	     */
	    sleep_time = sched_tick - thread->sched_stamp;
	    if ((thread->state & TH_RUN) == 0) {
		UPDATE_PRIORITY(thread);
	    }

	    /* fill in info */

	    thread_read_times(thread, &user_time, &system_time);
	    basic_info->user_time.seconds = user_time.seconds;
	    basic_info->user_time.microseconds =
				user_time.nanoseconds / 1000;
	    basic_info->system_time.seconds = system_time.seconds;
	    basic_info->system_time.microseconds =
				system_time.nanoseconds / 1000;

	    switch (thread->sched_policy->name) {
		case POLICY_BACKGROUND:
		    basic_info->base_priority = 32;
		    basic_info->cur_priority = 32;
		    break;

		case POLICY_TIMESHARE:
		{
		    struct policy_info_timeshare info;
		    natural_t count;

		    count = POLICY_INFO_TIMESHARE_COUNT;
		    (void) THREAD_GET_PARAM(thread,
					    (policy_param_t)&info,
					    &count);

		    basic_info->base_priority = info.base_priority;
		    basic_info->cur_priority  = info.cur_priority;
		    break;
		}

		default:
		    basic_info->base_priority = 0;
		    basic_info->cur_priority = 0;
		    break;
	    }

	    /*
	     *	To calculate cpu_usage, first correct for timer rate,
	     *	then for 5/8 ageing.  The correction factor [3/5] is
	     *	(1/(5/8) - 1).
	     */
	    basic_info->cpu_usage = thread->cpu_usage /
					(USAGE_RATE/TH_USAGE_SCALE);
	    basic_info->cpu_usage = (basic_info->cpu_usage * 3) / 5;

	    if (thread->state & TH_SWAPPED)
		flags = TH_FLAGS_SWAPPED;
	    else if (thread->state & TH_IDLE)
		flags = TH_FLAGS_IDLE;
	    else
		flags = 0;

	    if (thread->state & TH_HALTED)
		state = TH_STATE_HALTED;
	    else
	    if (thread->state & TH_RUN)
		state = TH_STATE_RUNNING;
	    else
	    if (thread->state & TH_UNINT)
		state = TH_STATE_UNINTERRUPTIBLE;
	    else
	    if (thread->state & TH_SUSP)
		state = TH_STATE_STOPPED;
	    else
	    if (thread->state & TH_WAIT)
		state = TH_STATE_WAITING;
	    else
		state = 0;		/* ? */

	    basic_info->run_state = state;
	    basic_info->flags = flags;
	    basic_info->suspend_count = thread->user_stop_count;
	    if (state == TH_STATE_RUNNING)
		basic_info->sleep_time = 0;
	    else
		basic_info->sleep_time = sleep_time;

	    thread_sched_unlock(thread);
	    splx(s);

	    *thread_info_count = THREAD_BASIC_INFO_COUNT;
	    return KERN_SUCCESS;
	}
	else if (flavor == THREAD_SCHED_INFO) {
	    thread_sched_info_t sched_info;

	    if (*thread_info_count < THREAD_SCHED_INFO_COUNT) {
		return KERN_INVALID_ARGUMENT;
	    }

	    sched_info = (thread_sched_info_t) thread_info_out;

	    s = splsched();
	    thread_sched_lock(thread);

	    sched_info->policy = thread->sched_policy->name;
	    switch (thread->sched_policy->name) {
		case POLICY_BACKGROUND:
		    sched_info->base_priority = 32;
		    sched_info->cur_priority = 32;
		    sched_info->max_priority = 32;
		    break;

		case POLICY_TIMESHARE:
		{
		    struct policy_info_timeshare info;
		    natural_t count;

		    count = POLICY_INFO_TIMESHARE_COUNT;
		    (void) THREAD_GET_PARAM(thread,
					    (policy_param_t) &info,
					    &count);

		    sched_info->base_priority = info.base_priority;
		    sched_info->cur_priority  = info.cur_priority;
		    sched_info->max_priority  = info.max_priority;
		}
		default:
			sched_info->base_priority = 0;
			sched_info->cur_priority = 0;
			sched_info->max_priority = 0;
		    break;
	    }

	    if (thread->cur_policy != thread->sched_policy) {
			sched_info->depressed = TRUE;
			sched_info->depress_priority = sched_info->cur_priority;
			sched_info->cur_priority = 32;
	    } else {
			sched_info->depressed = FALSE;
			sched_info->depress_priority = -1;
	    }

	    thread_sched_unlock(thread);
	    splx(s);

	    *thread_info_count = THREAD_SCHED_INFO_COUNT;
	    return KERN_SUCCESS;
	}
	else if (flavor == THREAD_POLICY_INFO) {
	    kern_return_t kr;
	    thread_policy_info_t policy_info;
	    natural_t policy_info_count;

	    if (*thread_info_count < THREAD_POLICY_INFO_COUNT)
		return KERN_INVALID_ARGUMENT;

	    policy_info = (thread_policy_info_t) thread_info_out;

	    s = splsched();
	    thread_sched_lock(thread);

	    policy_info->policy = thread->sched_policy->name;
	    policy_info->depressed =
		(thread->sched_policy != thread->cur_policy);

	    policy_info_count = *thread_info_count - THREAD_POLICY_INFO_COUNT;
	    if (policy_info_count > 0) {
		/*
		 *	There is room for the detailed scheduling policy
		 *	information.
		 */
		kr = THREAD_GET_PARAM(thread,
				      (policy_param_t) (policy_info + 1),
				      &policy_info_count);
		if (kr == KERN_SUCCESS)
		    *thread_info_count = THREAD_POLICY_INFO_COUNT +
			     policy_info_count;
	    }
	    else {
		*thread_info_count = THREAD_POLICY_INFO_COUNT;
		kr = KERN_SUCCESS;
	    }

	    thread_sched_unlock(thread);
	    splx(s);

	    return kr;
	}

	return KERN_INVALID_ARGUMENT;
}
