#ident "$Revision: 1.2 $"

AM_SRC_DEPTH = ..

include $(ROOT)/usr/include/make/commondefs
include $(AM_SRC_DEPTH)/include/amcommondefs
LDLIBS=-lssdb -lgen
LWOFF=,85,134

TARGETS = amconfig

default: $(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} $(IDB_TAG) -F $(ETCPATH) $(TARGETS) 
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amconfig amconfig
