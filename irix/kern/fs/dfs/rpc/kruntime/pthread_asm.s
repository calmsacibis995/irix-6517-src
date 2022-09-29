	.set noreorder
	.set noat
	.text

	.ent	kpth_save_stack_space
	.globl	kpth_save_stack_space
kpth_save_stack_space:
	addiu $1,$0,-16
	and $sp,$sp,$1
	addiu $1,$0,0xff0
	or $sp,$sp,$1
	j pthread_base
	nop
	.end	kpth_save_stack_space
