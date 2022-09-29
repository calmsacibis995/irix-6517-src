#!smake
#ident "$Revision: 1.7 $"

.NOTPARALLEL:
include $(ROOT)/usr/include/make/commondefs

SUBDIRS= wmisc wstring

default install $(COMMONTARGS)!
	$(SUBDIRS_MAKERULE)

.MAKE:default install $(COMMONTARGS)
