#!smake
#ident "$Revision: 1.1 $"

.NOTPARALLEL:
include $(ROOT)/usr/include/make/commondefs

SUBDIRS= cr1 cs des netdir netselect nsl rpc saf

default install $(COMMONTARGS)!
	$(SUBDIRS_MAKERULE)

${SUBDIRS}!.RECURSIVE

${SUBDIRS}!
	cd $@; exec ${MAKE}
