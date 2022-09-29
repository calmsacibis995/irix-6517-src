#!smake
#
# BSD print spooler makefile
#
# $Revision: 1.11 $

include $(ROOT)/usr/include/make/commondefs


CMNSRC	= common.c printcap.c displayq.c rmjob.c startdaemon.c
CMNOBJ	= $(CMNSRC:.c=.o)

LPDSRC	= lpd.c printjob.c recvjob.c lpdchar.c 
LPDOBJ	= $(LPDSRC:.c=.o)

LPCSRC	= lpc.c cmds.c cmdtab.c 
LPCOBJ	= $(LPCSRC:.c=.o)

LPRSRC	= lpr.c 
LPQSRC	= lpq.c 
LPRMSRC	= lprm.c 
PACSRC	= pac.c
LPTESTSRC= lptest.c
LPFSRC	= lpf.c 

CFILES=	$(CMNSRC) $(LPDSRC) $(LPRSRC) $(LPQSRC) $(LPRMSRC) \
	$(LPCSRC) $(LPTESTSRC) $(PACSRC) 

LCOPTS	= -signed
LCDEFS	= -D_BSD_SIGNALS
LLDLIBS	= -lbsd
LDIRT	= lplib.a

INST	= $(INSTALL) -idb "bsdlpr.sw"
SPGRP	= lp
ROOT_OWNER	= -u root -g $(SPGRP) -m 6711
SPGRP_OWNER	= -u bin  -g $(SPGRP) -m 2711
SPLDIR	= /var/spool/lpd
OLDSPLDIR = /usr/spool/lpd

TARGETS=lpr lpq lprm lpc lptest pac lpd lpf

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INST) -u $(SPGRP) -g $(SPGRP) -m 775 -dir $(SPLDIR)
	$(INST) -idb "delhist" -dir $(OLDSPLDIR)
	$(INST) $(ROOT_OWNER)  -F /usr/bsd lpr lpq lprm
	$(INST) $(ROOT_OWNER)  -F /usr/etc lpd
	$(INST) $(SPGRP_OWNER) -F /usr/etc lpc
	$(INST) -F /usr/bsd lptest
	$(INST) -F /usr/etc pac
	$(INST) -F /usr/lib lpf
	$(INST) -m 444 -idb "config(noupdate)" -F /etc \
		-src etc.printcap printcap

lplib.a: $(CMNOBJ)
	ar r $@ $?

lpd lpr lpq lprm lpc pac : lplib.a

lpd:	$(LPDOBJ) 
	$(CCF) $(LPDOBJ) lplib.a $(LDFLAGS) -o $@

lpc:	$(LPCOBJ)
	$(CCF) $(LPCOBJ) lplib.a $(LDFLAGS) -o $@

lpr lpq lprm pac: $$@.o
	$(CCF) $@.o lplib.a $(LDFLAGS) -o $@

lptest lpf: $$@.o
	$(CCF) $@.o $(LDFLAGS) -o $@
