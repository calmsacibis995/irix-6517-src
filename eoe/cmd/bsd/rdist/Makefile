#!smake
# Makefile for the 4.3BSD rdist command
#
# $Revision: 1.6 $$

BASEVERSION=n32bit
WANTPARALLEL=yes-please

.SUFFIXES:
.SUFFIXES:.o .c .y
include	${ROOT}/usr/include/make/cmdcommondefs

COMMANDS=rdist rdistd
CFILES  = common.c message.c setargs.c
CLIENT  = rdist.c client.c child.c distopt.c docmd.c expand.c \
		lookup.c isexec.c signal.c rshrcmd.c
SERVER  = rdistd.c server.c filesys.c filesys-os.c
YFILES	= gram.y
LDIRT   = gram.c

OCLIENT = $(YFILES:.y=.o) $(CFILES:.c=.o) $(CLIENT:.c=.o)
OSERVER = $(CFILES:.c=.o) $(SERVER:.c=.o)

LCDEFS += -D_BSD_SIGNALS -D_BSD_TIME -DOS_H=\"os-irix5.h\"
LCOPTS += -signed
INSTFLAGS = -m 0755 -u root -g sys
INSTBIN = /usr/bsd
YFLAGS = -d
LDIRT = $(YFILES:.y=.c)

default: $(TARGETS)

$(OCLIENT): y.tab.h

$(OSERVER): y.tab.h

y.tab.h: $(YFILEs:.y=.c)

include $(CMDCOMMONRULES)

rdist: $(OCLIENT)
	$(CCF) -o $@ $(OCLIENT) $(LDFLAGS)

rdistd: $(OSERVER)
	$(CCF) -o $@ $(OSERVER) $(LDFLAGS)

n32bitinstall: default
	${INSTALL} $(INSTFLAGS) -F $(INSTBIN) $(COMMANDS)
