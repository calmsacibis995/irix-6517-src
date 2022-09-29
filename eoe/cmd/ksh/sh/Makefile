#!smake
#	Makefile for ksh.
#

OBJECT_STYLE=N32_M3
include $(ROOT)/usr/include/make/commondefs

KSH     = sh builtin_exec
TARGETS = $(KSH)

GLDOPTS	=
LCINCS	= -I../include
#
# Specify load addrs for text and data to keep them in same segment with libc
# to save memory.  This might have to be bumped up in the future as things
# grow.
#
LLDOPTS = -Wl,-T,e000000,-D,e084000,-rdata_shared,-LD_LAYOUT:segalign=0x4000
LLDLIBS	= -nostdlib -L$(ROOT)/usr/lib32/mips3/nonshared -L$(ROOT)/usr/lib32/nonshared ../shlib/libsh.a -lc_nomp
LCDEFS	= -DKSHELL -DPROTO
LCOPTS  = -signed -non_shared -use_readonly_const
OPTIMIZER=-O3 -TARG:platform=ip22_4k
LWOFF	= ,1171

CFILES = args.c arith.c builtin.c cmd.c ctype.c defs.c echo.c \
	edit.c emacs.c error.c expand.c fault.c history.c \
	io.c jobs.c macro.c main.c msg.c name.c print.c service.c stak.c \
	string.c test.c vi.c word.c xec.c

default: $(TARGETS)

include $(COMMONRULES)

install: default 
	$(INSTALL) -F /sbin sh
	$(INSTALL) -F /sbin -lns sh rksh
	$(INSTALL) -F /sbin -lns sh ksh
	$(INSTALL) -F /usr/bin -lns ../../sbin/sh sh
	$(INSTALL) -F /usr/bin -lns ../../sbin/sh ksh
	$(INSTALL) -F /usr/bin -lns ../../sbin/sh rksh
	$(INSTALL) -F /usr/lib -lns ../../sbin/sh rsh
	$(INSTALL) -F /sbin builtin_exec
	$(INSTALL) -F /sbin -lns builtin_exec alias
	$(INSTALL) -F /sbin -lns builtin_exec bg
	$(INSTALL) -F /sbin -lns builtin_exec cd
	$(INSTALL) -F /sbin -lns builtin_exec command
	$(INSTALL) -F /sbin -lns builtin_exec fc
	$(INSTALL) -F /sbin -lns builtin_exec fg
	$(INSTALL) -F /sbin -lns builtin_exec getopts
	$(INSTALL) -F /sbin -lns builtin_exec hash
	$(INSTALL) -F /sbin -lns builtin_exec jobs
	$(INSTALL) -F /sbin -lns builtin_exec read
	$(INSTALL) -F /sbin -lns builtin_exec type
	$(INSTALL) -F /sbin -lns builtin_exec ulimit
	$(INSTALL) -F /sbin -lns builtin_exec umask
	$(INSTALL) -F /sbin -lns builtin_exec unalias
	$(INSTALL) -F /sbin -lns builtin_exec wait


sh: $(OBJECTS)
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@.precord
	${CORD} $@.precord $@.fb -o $@

builtin_exec:
	cp builtin_exec.sh builtin_exec

