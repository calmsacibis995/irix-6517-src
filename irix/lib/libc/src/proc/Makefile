#!smake

#ident "$Revision: 1.34 $"

DEPTH=..

include $(ROOT)/usr/include/make/libleafdefs

NOTYET=priocntl.c

ABI_CFILES= \
	execvp.c getgroups.c getpgid.c getpri.c getrusage.c getsid.c \
	memcntl.c mlock.c munlock.c  \
	setegid.c seteuid.c \
	setgroups.c setpgid.c setsid.c \
	uwait.c uwait3.c uwaitpid.c \
	waitid.c _wstat.c

NONABI_CFILES= BSDgetgroups.c BSDsetgroups.c \
	arsop.c \
	blockproc.c forkSCI.c \
	getash.c getprid.c getspinfo.c \
	mpin.c mlockall.c munlockall.c \
	newarraysess.c \
	pcreate.c \
	registerd_handles.c \
	rexecvp.c \
	setrgid.c setrlimitSCI.c setruid.c \
	setash.c setprid.c setspinfo.c \
	sprofil.c \
	waitsysSCI.c xpg4_setregid.c

ABI_ASFILES= \
	csu_tp.s execl.s execle.s execlp.s execv.s execve.s execve_tp.s \
	exit.s exit_tp.s fork.s fork_tp.s \
	getegid.s geteuid.s getuid.s getgid.s \
	getpid.s getppid.s getrlimit.s \
	profil.s \
	setgid.s setpgrp.s setregid.s setreuid.s setrlimit.s setuid.s \

NONABI_ASFILES=_sprocmonstart.s BSDgetpgrp.s BSDsetpgrp.s \
	moncontrol.s \
	pagelock.s pcreatel.s pcreatelp.s pcreateve_tp.s plock.s \
	prctl.s procblk.s \
	rexecl.s rexecle.s rexeclp.s rexecv.s rexecve.s \
	sproc.s sproc_tp.s \
	waitsys.s

ABI_32ASFILES= setrlimit64.s getrlimit64.s

#if $(LIBRARY) == "libc_32_abi.a"
CFILES= $(ABI_CFILES)
ASFILES=$(ABI_ASFILES) $(ABI_32ASFILES)
#elif $(LIBRARY) == "libc_64_abi.a"
CFILES= $(ABI_CFILES)
ASFILES=$(ABI_ASFILES)
#else
CFILES= $(ABI_CFILES) $(NONABI_CFILES)
ASFILES=$(ABI_ASFILES) $(NONABI_ASFILES)

#if $(OBJECT_STYLE:M32*) != ""
ASFILES+= $(ABI_32ASFILES)
#endif
#endif

include $(ROOT)/usr/include/make/libleafdefs
include $(LIBLEAFRULES)
