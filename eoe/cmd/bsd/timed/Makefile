#!smake
#
# Make the timed demon
#
# "$Revision: 1.20 $"

include	$(ROOT)/usr/include/make/commondefs

LCOPTS	=-signed -DHAVENIS -D_BSD_SIGNALS -D_BSD_TIME
LLDLIBS	=-lm

# source for timed
TDSRC=	acksend.c candidate.c correct.c \
	master.c networkdelta.c readmsg.c slave.c timed.c
TDOBJ=	$(TDSRC:.c=.o)

# source for timedc
TCSRC=	cmds.c cmdtab.c timedc.c
TCOBJ=	$(TCSRC:.c=.o)

# source for both timedc and timed
COMSRC=	byteorder.c measure.c cksum.c
COMOBJ=	$(COMSRC:.c=.o)

# source for timeslave
SLAVSRC=timeslave.c
SLAVOBJ=$(SLAVSRC:.c=.o)

CFILES=$(TDSRC) $(TCSRC) $(COMSRC) $(SLAVSRC)

TARGETS=timed timedc timeslave

default:$(TARGETS)

include $(COMMONRULES)

install:default
	$(INSTALL) -u root -g sys -m 4755 -F /usr/etc timedc
	$(INSTALL) -u root -g sys -m 755 -F /usr/etc \
		"timed timeslave"

timed:	$(TDOBJ) $(COMOBJ)
	$(CCF) $(TDOBJ) $(COMOBJ) $(LDFLAGS) -o $@
	$(CORD) $@ $@.fb
	mv $@.cord $@

timedc: $(TCOBJ) $(COMOBJ)
	$(CCF) $(TCOBJ) $(COMOBJ) $(LDFLAGS) -o $@

timeslave: $(SLAVOBJ)
	$(CCF) $(SLAVOBJ) $(LDFLAGS) -o $@
	$(CORD) $@ $@.fb
	mv $@.cord $@

lint:	$(_FORCE)
	$(LINT) $(LINTFLAGS) $(CDEFS) $(CINCS) $(TDSRC) $(COMSRC) $(LDLIBS)
	$(LINT) $(LINTFLAGS) $(CDEFS) $(CINCS) $(TCSRC) $(COMSRC) $(LDLIBS)
	$(LINT) $(LINTFLAGS) $(CDEFS) $(CINCS) $(SLAVSRC) $(LDLIBS)
