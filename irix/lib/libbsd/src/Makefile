#!smake
#ident "$Revision: 1.1 $"

.NOTPARALLEL:
include $(ROOT)/usr/include/make/commondefs

SUBDIRS= sys 

default install $(COMMONTARGS)!
	$(SUBDIRS_MAKERULE)

#${SUBDIRS}!.RECURSIVE

#${SUBDIRS}!
#	cd $@; exec ${MAKE}
