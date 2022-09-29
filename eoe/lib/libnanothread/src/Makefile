#!smake
#ident "$Revision: 1.3 $"

.NOTPARALLEL:
include $(ROOT)/usr/include/make/commondefs

SUBDIRS=nanothread autoheader

default install $(COMMONTARGS)!
	$(SUBDIRS_MAKERULE)

${SUBDIRS}!.RECURSIVE

${SUBDIRS}!
	cd $@; exec ${MAKE}
