#ident "$Revision: 1.4 $"

SSN_SRC_DEPTH = ..

include $(ROOT)/usr/include/make/commondefs
include $(SSN_SRC_DEPTH)/include/ssncommondefs

CFILES  = logstuff.c ssNotify.c

TARGETS	= espnotify
LDFLAGS = -lX11

default: $(TARGETS)

include $(COMMONRULES)

espnotify: $(OBJECTS) Makefile
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: default
	${INSTALL} $(IDB_TAG) -F $(BINPATH) $(TARGETS)
