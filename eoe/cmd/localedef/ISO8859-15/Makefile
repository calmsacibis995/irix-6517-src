#!smake

include $(ROOT)/usr/include/make/commondefs

COMMONPREF = loc

SUBDIRS != /bin/ls -d ??_??.*

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

include $(COMMONRULES)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)
