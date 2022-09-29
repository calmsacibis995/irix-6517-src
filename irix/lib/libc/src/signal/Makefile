#!smake

#ident "$Revision: 1.27 $"

DEPTH=..

include $(ROOT)/usr/include/make/libleafdefs

ABI_CFILES =  BSDsignal.c killpg.c makectxt.c \
	raise.c sigaction.c sigaltstack.c sigfillset.c siglongjmp.c \
	sigprocmask.c sigsetops.c sigsend.c sigstack.c \
	usignal.c usigignore.c usighold.c usigpause.c usigrelse.c usigset.c \
	usiginterrupt.c 

NONABI_CFILES= BSDlongjmp.c BSDsigpause.c \
	checkmask.c \
	psiginfo.c psignal.c \
	sgi_altersigs.c sgi_dumpset.c sgi_sigffset.c \
	siginfolst.c sigflag.c siglist.c \
	ssignal.c str2sig.c \
	sigblock.c \
	sigpendingSCI.c \
	sigpollSCI.c \
	sigsetmask.c \
	sigsuspendSCI.c \
	sigvec.c \
	ssig.c \
	sigqueue.c \
	sigtimedwait.c sigwaitrt.c sigwaitinfo.c sigwait.c xpg4_sigaltstack.c

ABI_ASFILES = \
	getcontext.s kill.s setlinkctxt.s \
	setcontext.s setjmp.s sigsendset.s swapctxt.s sigsetjmp.s \
	sigpending.s sigsuspend.s

NONABI_ASFILES=BSDsetjmp.s \
	ksigaction.s sigpoll.s sigtramp.s \
	ksigqueue.s ksigprocmask.s sigset.s

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES= $(ABI_CFILES)
ASFILES=$(ABI_ASFILES)
#else
CFILES= $(ABI_CFILES) $(NONABI_CFILES)
ASFILES=$(ABI_ASFILES) $(NONABI_ASFILES)
#endif

include $(LIBLEAFRULES)
