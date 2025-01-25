.section .data
.align 4
.globl icode
.globl szicode

/
/ This code is the init process; it is copied to user text space
/ and it then does exec("/sbin/init", "/sbin/init", 0);
/
icode:
	movl $UVTEXT, %eax
	leal argv_off(%eax), %ebx
	pushl %ebx
	leal sbin_off(%eax), %ebx
	pushl %ebx
	pushl $0
	movl $11, %eax		/ 0x11 is execl()
	lcall $USER_SCALL, $0
	movl $UVTEXT, %eax
	pushl $2
	leal sysmsg_off(%eax), %ebx
	pushl %ebx
	pushl $0
	movl $5, %eax		/ 0x05 is open()
	lcall $USER_SCALL, $0
	movl %eax, %esi
	movl $UVTEXT, %eax
	pusl $ierr_len
	leal ierr_off(%eax), %ebx
	pushl %ebx
	pushl %esi
	pushl $0
	movl $4, %eax		/ 0x04 is write()
	lcall $USER_SCALL, $0
	movl $UVTEXT, %eax
	pushl %esi
	pushl $0
	movl $6, %eax		/ 0x06 is close()
	lcall $USER_SCALL, $0
	jmp .

.align 4

argv:
	.set argv_off, argv - icode
	.long UVTEXT+sbin_off
	.long 0

sbin_init:
	.set sbin_off, sbin_init - icode
	.string "/sbin/init"

sysmsg:
	.set sysmsg_off, sysmsg - icode
	.string "/dev/sysmsg"

ierr_msg:
	.set ierr_off, ierr_msg, icode
	.string "main: Can't exec /sbin/init\n\n"
	.set ierr_len, . - ierr_msg - 1

icode_end:

.align 4

szicode:
	.long icode_end - icode
