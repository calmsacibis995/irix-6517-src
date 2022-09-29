#ident "$Revision: 1.2 $"

AM_SRC_DEPTH = ..

include $(ROOT)/usr/include/make/commondefs
include $(AM_SRC_DEPTH)/include/amcommondefs

LCOPTS  = -woff 85,134
LCINCS  = -I$(ROOT)/usr/include/sss -I../include
TARGETS = amconvert
LDLIBS  = -lssdb

default: $(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} $(IDB_TAG) -F $(ETCPATH) $(TARGETS) 
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amconvert amconvert
