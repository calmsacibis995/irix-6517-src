#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=pathchk.c	

TARGETS=pathchk

LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin $(TARGETS)

pathchk:$(OBJECTS) 
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)

