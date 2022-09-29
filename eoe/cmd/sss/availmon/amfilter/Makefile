#ident "$Revision: 1.1 $"

AM_SRC_DEPTH = ..

include $(ROOT)/usr/include/make/commondefs
include $(AM_SRC_DEPTH)/include/amcommondefs

TARGETS = amfilter

default: $(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} $(IDB_TAG) -F $(ETCPATH) $(TARGETS) 
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amfilter amfilter
