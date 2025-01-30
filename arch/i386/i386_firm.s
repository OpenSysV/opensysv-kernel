/ Return to firmware.
.globl rtnfirm
.globl inrtnfirm

.extern haltsys

.section .text
.align 4

rtnfirm:
	movl $1, inrtnfirm
	call spl0
loop:
	jmp loop

.section .data
inrtnfirm:
	.long 0
