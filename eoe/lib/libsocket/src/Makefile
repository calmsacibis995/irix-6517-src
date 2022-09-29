#!smake
#ident "$Revision: 1.1 $"

.NOTPARALLEL:
include $(ROOT)/usr/include/make/commondefs

SUBDIRS=socket inet

default install $(COMMONTARGS)!
	$(SUBDIRS_MAKERULE)

${SUBDIRS}!.RECURSIVE

${SUBDIRS}!
	cd $@; exec ${MAKE}
