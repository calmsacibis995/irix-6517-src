#!smake
#ident	"$Revision: 1.54 $"

OBJECT_STYLE=N32_M3
VLLDLIBS=-L$(ROOT)/usr/lib32/mips3/nonshared -L$(ROOT)/usr/lib32/nonshared -lgen -lw -lc_nomp
VLCOPTS=-woff 1116
WANTPARALLEL=yes-please

include	$(ROOT)/usr/include/make/commondefs

# Compile Time Options
#
#	-DNICE		if defined then a backround process will be
#			niced down to NICE.  
#	-DACCT		if defined, then shell accounting will be done.
#			Note that the rule line for service.o needs to
#			be modified if this is included.
#

TARGETS=bsh 

CFILES =  args.c blok.c cmd.c ctype.c \
	defs.c echo.c error.c expand.c \
	fault.c func.c hash.c hashserv.c io.c macro.c main.c msg.c name.c \
	print.c pwd.c service.c setbrk.c stak.c string.c test.c word.c \
	xec.c bltin.c jobs.c ulimit.c resource.c libc.c

GLDOPTS=
LCOPTS=-G 4 -non_shared -use_readonly_const $(VLCOPTS)
# Link text and data close together.
LLDOPTS=-Wl,-T,10000000,-rdata_shared

LLDLIBS=-nostdlib $(VLLDLIBS)
LCDEFS= -DACCT

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin bsh
	$(INSTALL) -F /sbin -lns bsh jsh
	$(INSTALL) -F /usr/bin -lns ../../sbin/bsh jsh
	$(INSTALL) -F /usr/bin -lns ../../sbin/bsh bsh

bsh: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@.precord
	${CORD} $@.precord $@.fb -o $@
