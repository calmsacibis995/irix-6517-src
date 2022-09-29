#!smake

include	$(ROOT)/usr/include/make/commondefs

TARGETS=rtmon-client rtmon-dump rtmon-run

LCDEFS+=-D_KMEMUSER
LCOPTS+=-fullwarn
#LLDLIBS= -L$$ROOT/usr/lib32 -lrtmon -lc
LLDLIBS= -lrtmon -lc

OPTIMIZER = -g
CLIENTFILES = client.c util.c
PARSERFILES = rtdump.c util.c
RUNFILES = rtrun.c
WVFILES = wvdump.c
CFILES = $(CLIENTFILES) $(PARSERFILES) ${WVFILES} ${RUNFILES}

default: $(TARGETS)

rtmon-client: ${CLIENTFILES:.c=.o}
	$(CCF) -o $@ ${CLIENTFILES:.c=.o} $(LDOPTS) $(LLDLIBS)

rtmon-dump: ${PARSERFILES:.c=.o}
	$(CCF) -o $@ $(PARSERFILES:.c=.o) $(LDOPTS) $(LLDLIBS)
rtmon-run:	${RUNFILES:.c=.o}
	$(CCF) -o $@ $(RUNFILES:.c=.o)
wvdump: ${WVFILES:.c=.o}
	$(CCF) -o $@ $(WVFILES:.c=.o)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)
