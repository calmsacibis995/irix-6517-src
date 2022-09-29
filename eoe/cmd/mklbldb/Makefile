#!smake
# $Revision: 1.1 $

include	$(ROOT)/usr/include/make/commondefs

CFILES= mklbldb.c mls_init.c mls_mkdb.c mls_unique.c
TARGETS=mklbldb
LLDLIBS =

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)

mklbldb: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@
