#
#ident "$Revision: 1.7 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=montbl LC_MONETARY hostmontbl
LDIRT=

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin montbl
	${INSTALL} -F /usr/lib/locale/C montbl_C
	${INSTALL} -F /usr/lib/locale/C LC_MONETARY

LC_MONETARY:hostmontbl
	./hostmontbl montbl_C

hostmontbl:hostmontbl.o
	$(HOST_CC) $(MKDEPOPT) -o $@ hostmontbl.o
hostmontbl.o:montbl.c
	$(HOST_CC) $(MKDEPOPT) -c -o $@ montbl.c
