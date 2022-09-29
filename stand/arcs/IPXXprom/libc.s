/* local system calls so we don't have to link with -lc
 */
#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

#define LSYSCALL(name,call)			\
LEAF(name);					\
	li	v0,SYS_/**/call;		\
	syscall;		 		\
	beq	a3,zero,9f;			\
	j	_lcerror;			\
9:

LSYSCALL(l_ioctl,ioctl)
	RET(l_ioctl)

LSYSCALL(l_select,select)
	RET(l_select)

LSYSCALL(l_read,read)
	RET(l_read)

LSYSCALL(l_write,write)
	RET(l_write)

LSYSCALL(l_pipe,pipe)
	sw	v0,0(a0)
	sw	v1,4(a0)
	move	v0,zero
	RET(l_pipe)

LSYSCALL(l_open,open)
	RET(l_open)

LSYSCALL(l_close,close)
	RET(close)

LSYSCALL(l_dup,dup)
	RET(l_dup)

LSYSCALL(l_exit,exit)
	RET(l_exit)

LSYSCALL(l_execve,execve)
	RET(l_execve)

LSYSCALL(l_sginap,sginap)
	RET(l_sginap)

LSYSCALL(l_fork,fork)
	beq	v1,zero,1f
	move	v0,zero
1f:
	RET(l_fork);

.globl	_cerror
_lcerror:
_cerror:
	sw	v0,errno
	li	v0,-1
	j	ra
