haltsys:
	cli		/ Clear interrupts.
	hlt		/ Halt.
	jmp haltsys	/ If we somehow break from infinite loop, jump back in.
