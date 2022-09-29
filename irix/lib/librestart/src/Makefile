#!smake
#ident "$Revision: 1.36 $"

DEPTH=.
SM=:
include $(ROOT)/usr/include/make/libleafdefs


ASFILES= \
	atomic.s \
	cachectl.s \
	cerror.s \
	ckpt_sbrk.s \
	chmod.s \
	close.s \
	ckpt_exit.s \
	getcontext.s \
	ioctl.s \
	kill.s \
	lseek.s \
	mmap.s \
	mprotect.s \
	munmap.s \
	nsproctid.s \
	open.s \
	pagelock.s \
	plock.s \
	prctl.s \
	procblk.s \
	read.s \
	readv.s \
	restart.s \
	setcontext.s \
	setrlimit.s \
	setitimer.s \
	stack.s \
	syscall.s \
	syssgi.s \
	unlink.s \
	usync_cntl.s \
	waitsys.s \
	write.s \
	xstat.s \
	geteuid.s \
	setreuid.s \
	ksigaction.s

CFILES= ckpt_foster.c \
	ckpt_mp.c \
	ckpt_pthread.c \
	ckpt_remap.c \
	ckpt_revision.c \
	ckpt_util.c \
	blockproc.c \
	mpin.c \
	shmsys.c \
	sysmp.c \
	uwait.c \
	_wstat.c

LCOPTS+=-g3
LCDEFS= -D_KMEMUSER -DCKPT -DNDEBUG
LWOFF=,1209,1509

include $(LIBLEAFRULES)

