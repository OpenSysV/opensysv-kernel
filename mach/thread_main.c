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
 *	File:	mach/thread/thread_main.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young, David Golub
 *	Date:	1986
 *
 *	Thread management primitives implementation.
 */

void
thread_init(void)
{
	thread_zone = zinit(sizeof(struct thread),
		THREAD_MAX * sizeof(struct thread),
		THREAD_CHUNK * sizeof(struct thread), FALSE, "threads");

	/*
	 *	Fill in a template thread for fast initialization.
	 *	[Fields that must be (or are typically) reset at
	 *	time of creation are so noted.]
	 */

	/* thread_template.links (none) */
	thread_template.runq = RUN_QUEUE_HEAD_NULL;

	/* thread_template.task (later) */
	/* thread_template.thread_list (later) */
	/* thread_template.pset_threads (later) */

	/* one ref for being alive; one for the guy who creates the thread */
	thread_template.ref_count = 2;
	/* thread_template.ref_lock (later) */
	/* thread_template.lock (later) */

	/* thread_template.pcb (later) */
	thread_template.kernel_stack = (vm_offset_t) 0;
	thread_template.stack_privilege = (vm_offset_t) 0;

	thread_template.swap_func = thread_bootstrap_return;

	/* thread_template.sched_lock (later) */
	thread_template.wait_event = 0;
	/* thread_template.suspend_count (later) */
	thread_template.wait_result = KERN_SUCCESS;
	thread_template.suspend_wait = FALSE;
	thread_template.state = TH_SUSP | TH_SWAPPED;

	thread_template.active = FALSE; /* reset */
	thread_template.ast = AST_ZILCH;

	thread_template.user_stop_count = 1;

	timer_init(&(thread_template.user_timer));
	timer_init(&(thread_template.system_timer));
	thread_template.user_timer_save.low = 0;
	thread_template.user_timer_save.high = 0;
	thread_template.system_timer_save.low = 0;
	thread_template.system_timer_save.high = 0;
	thread_template.cpu_delta = 0;
	thread_template.sched_delta = 0;
	thread_template.cpu_usage = 0;
	thread_template.sched_usage = 0;
	/* thread_template.sched_stamp (later) */

	thread_template.recover = (vm_offset_t) 0;
	thread_template.vm_privilege = FALSE;

	/* thread_template.<IPC structures> (later) */

	/* thread_template.processor_set (later) */
#if	NCPUS > 1
	thread_template.bound_processor = PROCESSOR_NULL;
	/* thread_template.last_processor  (later) */
#endif

/*	thread_template.policy_index (later) */
	thread_template.cur_policy = 0;
/*	thread_template.sched_policy (later) */

	/*
	 *	Initialize other data structures used in
	 *	this module.
	 */

	queue_init(&reaper_queue);
	simple_lock_init(&reaper_lock);

	simple_lock_init(&stack_lock_data);

	/*
	 *	Initialize any machine-dependent
	 *	per-thread structures necessary.
	 */

	pcb_module_init();
}

kern_return_t
thread_create(task_t parent_task, thread_t *child_thread)
{
	thread_t new_thread;
	processor_set_t pset;

	if (parent_task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	/*
	 *	Allocate a thread and initialize static fields
	 */

	new_thread = (thread_t) zalloc(thread_zone);

	if (new_thread == THREAD_NULL)
		return KERN_RESOURCE_SHORTAGE;

	*new_thread = thread_template;

	/*
	 *	Initialize runtime-dependent fields
	 */

	new_thread->task = parent_task;
	simple_lock_init(&new_thread->lock);
	simple_lock_init(&new_thread->ref_lock);
	simple_lock_init(&new_thread->sched_lock);
	new_thread->sched_stamp = sched_tick;
	new_thread->timer.te_param = new_thread;
	new_thread->timer.te_clock = sys_clock;

	/*
	 *	Create a pcb.  The kernel stack is created later,
	 *	when the thread is swapped-in.
	 */
	pcb_init(new_thread);

	ipc_thread_init(new_thread);

	/*
	 *	Find the processor set for the parent task.
	 */
	task_lock(parent_task);
	if (!parent_task->active) {
	    /*
	     *	Parent task is being shut down.  Quit now.
	     */
	    ipc_thread_terminate(new_thread);
	    pcb_terminate(new_thread);
	    zfree(thread_zone, (vm_offset_t) new_thread);

	    return KERN_FAILURE;
	}
	     
	pset = parent_task->processor_set;
	pset_lock(pset);

	/*
	 *	Set the thread`s scheduling policy and parameters
	 *	to the default for the task.
	 */
	new_thread->processor_set = pset;	/* for sched_ops to work */

	thread_set_initial_policy(new_thread, parent_task);

	/*
	 *	Thread is suspended if the task is.  Add 1 to
	 *	suspend count since thread is created in suspended
	 *	state.
	 */
	new_thread->suspend_count = parent_task->suspend_count + 1;

	/*
	 *	Add the thread to the processor set.
	 *	If the pset is empty, suspend the thread again.
	 */

	pset_reference(pset);		/* thread`s ref to pset */
	pset_add_thread(pset, new_thread);
	if (pset->empty)
		new_thread->suspend_count++;

	/*
	 *	Add the thread to the task`s list of threads.
	 *	The new thread holds another reference to the task.
	 */

	parent_task->ref_count++;

	parent_task->thread_count++;
	queue_enter(&parent_task->thread_list, new_thread, thread_t,
					thread_list);

	/*
	 *	Finally, mark the thread active.
	 */

	new_thread->active = TRUE;

	task_unlock(parent_task);
	pset_unlock(pset);

	ipc_thread_enable(new_thread);

	*child_thread = new_thread;
	return KERN_SUCCESS;
}

/*
 *	thread_terminate:
 *
 *	Permanently stop execution of the specified thread.
 *
 *	A thread to be terminated must be allowed to clean up any state
 *	that it has before it exits.  The thread is broken out of any
 *	wait condition that it is in, and signalled to exit.  It then
 *	cleans up its state and calls thread_halt_self on its way out of
 *	the kernel.  The caller waits for the thread to halt, terminates
 *	its IPC state, and then deallocates it.
 *
 *	If the caller is the current thread, it must still exit the kernel
 *	to clean up any state (thread and port references, messages, etc).
 *	When it exits the kernel, it then terminates its IPC state and
 *	queues itself for the reaper thread, which will wait for the thread
 *	to stop and then deallocate it.  (A thread cannot deallocate itself,
 *	since it needs a kernel stack to execute.)
 */
kern_return_t
thread_terminate(thread_t thread)
{
	thread_t cur_thread = current_thread();
	task_t cur_task;
	spl_t s;

	if (thread == THREAD_NULL)
		return KERN_INVALID_ARGUMENT;

	if (thread == cur_thread) {

	    thread_lock(thread);
	    if (thread->active) {
		/*
		 *	Curreht thread is active, and is terminating
		 *	itself.  Make thread queue itself for reaper
		 *	when exiting kernel.  Reaper will do the rest
		 *	of the work.
		 */
		thread->active = FALSE;
		ipc_thread_disable(thread);

		s = splsched();
		thread_sched_lock(thread);
		thread_ast_set(thread, AST_TERMINATE);	/* queue for reaper */
		thread_sched_unlock(thread);

		ast_on(cpu_number(), AST_TERMINATE);
		splx(s);

		thread_unlock(thread);
		return KERN_SUCCESS;
	    }
	    else {
		/*
		 *	Someone else is already terminating the
		 *	current thread.  Just make it halt.  The
		 *	thread that is calling thread_terminate
		 *	is (or will be) waiting	for this one to
		 *	halt, and will do the rest of the work.
		 *
		 *	Hold the thread, because the thread that
		 *	is calling thread_terminate may not yet
		 *	have called thread_halt on this thread.
		 */
		thread_hold(thread);

		s = splsched();
		thread_sched_lock(thread);
		thread_ast_set(thread, AST_HALT);	/* don`t queue
							   for reaper */
		thread_sched_unlock(thread);

		ast_on(cpu_number(), AST_HALT);
		splx(s);

		thread_unlock(thread);
		return KERN_FAILURE;
	    }
	}

	/*
	 *	Lock both threads and the current task
	 *	to check termination races and prevent deadlocks.
	 */
	cur_task = cur_thread->task;
	task_lock(cur_task);

	if ((vm_offset_t)thread < (vm_offset_t)cur_thread) {
		thread_lock(thread);
		thread_lock(cur_thread);
	}
	else {
		thread_lock(cur_thread);
		thread_lock(thread);
	}

	/*
	 *	If the current thread is being terminated, help out.
	 */
	if (!cur_task->active || !cur_thread->active) {
		thread_unlock(cur_thread);
		thread_unlock(thread);

		task_unlock(cur_task);
		(void) thread_terminate(cur_thread);
		return KERN_FAILURE;
	}
    
	thread_unlock(cur_thread);
	task_unlock(cur_task);

	/*
	 *	Terminate victim thread.
	 */
	if (!thread->active) {
		/*
		 *	Someone else got there first.
		 */
		thread_unlock(thread);

		return KERN_FAILURE;
	}

	/*
	 *	Mark thread inactive, and disable IPC access.
	 */
	thread->active = FALSE;
	ipc_thread_disable(thread);

	thread_unlock(thread);

	/*
	 *	Terminate the thread.
	 */
	thread_terminate_internal(thread);

	return KERN_SUCCESS;
}

/*
 *	thread_force_terminate:
 *
 *	Version of thread_terminate called by task_terminate.  thread is
 *	not the current thread.  task_terminate is the dominant operation,
 *	so we can force this thread to stop.
 */
void
thread_force_terminate(thread_t thread)
{
	/*
	 *	If thread is already being shut down,
	 *	we don`t have to do anything.
	 */
	thread_lock(thread);
	if (!thread->active) {
	    thread_unlock(thread);
	    return;
	}

	/*
	 *	Mark thread inactive, and shut down its IPC
	 *	control.
	 */
	thread->active = FALSE;
	ipc_thread_disable(thread);

	thread_unlock(thread);

	/*
	 *	Terminate the thread.
	 */
	thread_terminate_internal(thread);
}

no_return
walking_zombie(void)
{
	for (;;)
	    panic("the zombie walks!");
}

/*
 *	A thread can only terminate itself when it has
 *	hit a clean point.  It calls this function to
 *	mark itself as halted, and queue itself for the
 *	reaper thread.  The reaper thread actually
 *	cleans up the thread.
 *
 *	Thread is already marked inactive.
 */
no_return
thread_terminate_self(void)
{
	thread_t thread = current_thread();
	spl_t s;

	thread_hold(thread);

	s = splsched();
	thread_sched_lock(thread);
	thread->state |= TH_HALTED;
	thread_sched_unlock(thread);
	splx(s);

	simple_lock(&reaper_lock);
	enqueue_tail(&reaper_queue, (queue_entry_t) thread);
	simple_unlock(&reaper_lock);
	thread_wakeup(&reaper_queue);

	counter(c_thread_halt_self_block++);
	thread_block_noreturn(walking_zombie);
	/*NOTREACHED*/
}

