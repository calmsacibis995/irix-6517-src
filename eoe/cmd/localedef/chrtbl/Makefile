#
#ident "$Revision: 1.6 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=chrtbl LC_CTYPE LC_NUMERIC hostchrtbl
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin chrtbl
	${INSTALL} -F /usr/lib/locale/C chrtbl_C
	${INSTALL} -F /usr/lib/locale/C LC_CTYPE
	${INSTALL} -F /usr/lib/locale/C LC_NUMERIC

LC_CTYPE LC_NUMERIC:hostchrtbl
	./hostchrtbl chrtbl_C

hostchrtbl: hostchrtbl.o
	$(HOST_CC) $(MKDEPOPT) -o $@ hostchrtbl.o

hostchrtbl.o: chrtbl.c
	$(HOST_CC) $(MKDEPOPT) -o $@ -c chrtbl.c
