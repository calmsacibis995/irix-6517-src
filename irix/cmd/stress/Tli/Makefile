#!smake
#
# $Id: Makefile,v 1.1 1997/05/13 20:53:51 sca Exp $

BASEVERSION=32bit
ALTVERSIONS=n32bit

include $(ROOT)/usr/include/make/cmdcommondefs

DIR=Tli

COMMANDS=ls lc is ic netver lcclose icclose runtests

# -g is really useful for debugging ... (and -g3 doesn't help much...)
LCOPTS+=-g -fullwarn
LDLIBS+=-lnsl
OPTIMIZER=
#
# 84 - stop whining about not using anything from libstress.a
LLDOPTS=-Wl,-woff,84
WOFF=

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/${DIR} '$(COMMANDS)'
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/${DIR} '$(COMMANDS)'

64bitinstall: default
	$(INSTALL) -idb "nostrip" $(IDB_TAG64) -F /usr/stress/${DIR}/64bit '$(COMMANDS)'
	$(INSTALL) -idb "nostrip" $(IDB_TAG64) -F /usr/diags/crash/${DIR}/64bit '$(COMMANDS)'

n32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/${DIR}/n32 $(COMMANDS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/${DIR}/n32 $(COMMANDS)
