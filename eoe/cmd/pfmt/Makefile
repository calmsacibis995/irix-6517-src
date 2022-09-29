#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=pfmt lfmt
LLDLIBS=-lgen

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin pfmt
	${INSTALL} -F /sbin lfmt
	${INSTALL} -F /usr/bin -lns ../../sbin/lfmt lfmt

lfmt: lfmt.o
	$(CCF) -DLFMT lfmt.o -o lfmt $(LDFLAGS)
lfmt.o:
	$(CCF) -c -DLFMT pfmt.c -o lfmt.o 
