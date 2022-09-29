# $Revision: 1.3 $

include $(ROOT)/usr/include/make/commondefs

SUBDIRS=session

CFILES=usemem.c dofuser.c
SHFILES=
TARGETS= ${SHFILES:.sh=} ${CFILES:.c=}

default all: $(TARGETS)
	$(SUBDIRS_MAKERULE)

COMMONPREF=cmd_
include $(COMMONRULES)

$(COMMONTARGS) install: $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

$(COMMONPREF)install:$(TARGETS)
	$(INSTALL) -idb "nostrip" -F /usr/stress/cmd '$(TARGETS)'

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)
