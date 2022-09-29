#!smake
#
# Makefile for os which contains generic kernel objects.
#
#ident	"$Revision: 3.139 $"

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os
.PATH:$(KPATH)
SUBDIRS=scheduler ksync numa shm as cpr proc vm file host pagg
HEADERS_SUBDIRS=scheduler ksync cpr vm numa

#if defined(CELL)
SUBDIRS+=cell
#endif

OS_NAME=os.a
OS_CSRCS=acct.c arsess.c as_scalls.c avl.c \
	behavior.c bitmasks.c \
	clock.c cred.c \
	dba.c dnlc.c \
	exec.c elf.c elf64.c exit.c \
	fdt.c flock.c flock_new.c fork.c fs_subr.c fuexarg.c fuexarg64.c \
	getpages.c rmap.c \
	handle.c \
	intp.c invent.c \
	kabi.c kopt.c ksynch.c kthread.c ktrace.c \
	lookup.c lpage.c\
	machdep.c main.c malloc.c melf.c mload.c move.c \
	name.c \
	page.c pathname.c pda.c physio.c pipe.c \
        printf.c probe.c procset.c prsched.c ptimers.c \
	resource.c \
	scalls.c sched.c shm_syscalls.c sig.c slp.c space.c \
	startup.c \
	sthread.c subr.c swapalloc.c swtch.c \
	sysent.c sysget.c sysmips.c sysmp.c syssgi.c \
	thread.c tile.c time.c timers.c tlbmgr.c trap.c tstamp.c \
	uidact.c uli.c uthread.c utssys.c uuid.c \
	vfs.c vmdump.c vncalls.c vnode.c vn_passthru.c \
	watchpoints.c xthread.c \
	mtext.c

OS_OBJS=${OS_CSRCS:.c=.o}
$(OS_NAME):$(OS_NAME)($(OS_OBJS)) MAKELIB

SUSER_NAME=suser.a
SUSER_CSRCS=suser.c
SUSER_OBJS=${SUSER_CSRCS:.c=.o}
$(SUSER_NAME):$(SUSER_NAME)($(SUSER_OBJS)) MAKELIB

MAC_NAME=mac.a
MAC_CSRCS=mac.c mac_add_label.c mac_dom.c mac_dup.c mac_invalid.c mac_label.c
MAC_OBJS=${MAC_CSRCS:.c=.o}
$(MAC_NAME):$(MAC_NAME)($(MAC_OBJS)) MAKELIB

CAP_MAC_ACCESS_NAME=cap_mac_access.a
CAP_MAC_ACCESS_CSRCS= cap_mac_access.c
CAP_MAC_ACCESS_OBJS=${CAP_MAC_ACCESS_CSRCS:.c=.o}
$(CAP_MAC_ACCESS_NAME):$(CAP_MAC_ACCESS_NAME)($(CAP_MAC_ACCESS_OBJS)) MAKELIB

SAT_NAME=sat.a
SAT_CSRCS= sat.c
SAT_OBJS=${SAT_CSRCS:.c=.o}
$(SAT_NAME):$(SAT_NAME)($(SAT_OBJS)) MAKELIB

CAP_NAME=capability.a
CAP_CSRCS= capability.c cap_able.c
CAP_OBJS=${CAP_CSRCS:.c=.o}
$(CAP_NAME):$(CAP_NAME)($(CAP_OBJS)) MAKELIB

ACL_NAME=acl.a
ACL_CSRCS= acl.c
ACL_OBJS=${ACL_CSRCS:.c=.o}
$(ACL_NAME):$(ACL_NAME)($(ACL_OBJS)) MAKELIB

EAG_NAME=eag.a
EAG_CSRCS= eag.c
EAG_OBJS=${EAG_CSRCS:.c=.o}
$(EAG_NAME):$(EAG_NAME)($(EAG_OBJS)) MAKELIB

MIDBG_NAME=mloadidbg.o
MIDBG_CSRCS=mloadidbg.c
MIDBG_OBJS=${MIDBG_CSRCS:.c=.o}

PRSGI_NAME=prsgi.o
PRSGI_CSRCS=prsgi.c
PRSGI_OBJS=${PRSGI_CSRCS:.c=.o}

UNC_NAME=uncintf.o
UNC_CSRCS=uncintf.c
UNC_OBJS=${UNC_CSRCS:.c=.o}

#if defined(CPUBOARD) && ($(CPUBOARD) == "IP19" || $(CPUBOARD) == "IP25" || $(CPUBOARD) == "IP27")
KLISTIO_NAME=klistio.a
KLISTIO_CSRCS=klistio.c
KLISTIO_OBJS=${KLISTIO_CSRCS:.c=.o}
$(KLISTIO_NAME):$(KLISTIO_NAME)($(KLISTIO_OBJS)) MAKELIB
#endif

TARGETS=$(OS_NAME) $(MAC_NAME) $(EAG_NAME) $(SAT_NAME) \
	$(ACL_NAME) $(CAP_NAME) \
	$(CAP_MAC_ACCESS_NAME) $(KLISTIO_NAME) \
	$(SUSER_NAME) $(MIDBG_NAME) $(PRSGI_NAME) $(UNC_NAME)
.PRECIOUS:$(TARGETS)

# lint
LINTNAME=os
CFILES=$(OS_CSRCS) $(MAC_CSRCS) $(EAG_CSRCS) \
	$(CAP_MAC_ACCESS_CSRCS) \
	$(KLISTIO_CSRCS) $(SUSER_CSRCS) $(MIDBG_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

# This default rule is always invoked in BOOTAREA
$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} $(TARGETS:S/prsgi.o//)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} $(TARGETS:S/prsgi.o//)
	${KINSTALL} ${MIDBG_NAME}
	${KDEVINSTALL} $(TARGETS:S/prsgi.o//)

# install header files under ${ROOT}/usr/include
headers!
	${HEADERS_SUBDIRS_MAKERULE}

# space.c is compiled with -g in order to force symbol table debug information
# to be loaded for all of the kernel structures which are referenced there.
#
#if $(VCC) == "CFE"
space.o: space.c
	$(CC) $(CFLAGS) -g -c $(.IMPSRC) && \
	$(LDF) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

elf64.o: elf.c
	$(CC) $(CFLAGS) -DELF64 -c $(KPATH)/elf.c -o $(.TARGET) && \
	$(LDF) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

fuexarg64.o: fuexarg.c
	$(CC) $(CFLAGS) -DELF64 -c $(KPATH)/fuexarg.c -o $(.TARGET) && \
	$(LDF) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

mloadidbg.o: mloadidbg.c
	$(CCF) -c $(JALR) -g -G 0 $(KPATH)/mloadidbg.c && \
	$(LDF) $(PRODOBJECT) -G 0 -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

#else

space.o: space.c
	$(CC) $(CFLAGS) -g -c $(.IMPSRC)

elf64.o: elf.c
	$(CC) $(CFLAGS) -DELF64 -c $(KPATH)/elf.c -o $(.TARGET)

fuexarg64.o: fuexarg.c
	$(CC) $(CFLAGS) -DELF64 -c $(KPATH)/fuexarg.c -o $(.TARGET)

mloadidbg.o: mloadidbg.c
	$(CCF) -c $(JALR) -g -G 0 $(KPATH)/mloadidbg.c

#endif

# hide as many symbols as possible.
prsgi.o: $(KPATH)/$(.PREFIX).c
	$(CCF) -c $(KPATH)/$*.c && \
	$(LDF) -r $(STFLAG) $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

