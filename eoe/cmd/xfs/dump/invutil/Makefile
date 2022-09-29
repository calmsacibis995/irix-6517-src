#!smake
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

COMMINCL = \
	global.h \
	mlog.h \
	util.h \
	types.h

INVINCL = \
	inventory.h \
	inv_priv.h

INVCOMMON = 

COMMON = 

LOCALS = invutil.c 

CFILES = $(COMMON) $(INVCOMMON) $(LOCALS)
OFILES = $(CFILES:.c=.o)
LINKS = $(COMMINCL) $(COMMON) $(INVINCL) $(INVCOMMON)

LCOPTS = -fullwarn
LCDEFS = -D_SGI_SOURCE -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS -DDUMP -DRMT -DCLRMTAUD -DEXTATTR -DBASED -DDOSOCKS -DINVCONVFIX -DSIZEEST -DPIPEINVFIX -DBANYAN -DFICUS -DNDEBUG
LLDLIBS = 
LDIRT = $(LINKS)
WOFF =

TARGETS = xfsinvutil

default: $(LINKS) $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)

xfsinvutil: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

$(COMMINCL) $(COMMON):
	@rm -f $@; ln -s ../common/$@ $@

$(INVINCL) $(INVCOMMON):
	@rm -f $@; ln -s ../inventory/$@ $@

