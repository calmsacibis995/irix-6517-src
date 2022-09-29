# $Revision: 1.4 $

include $(ROOT)/usr/include/make/commondefs

SUBDIRS = generic lpg numa
CFILES =
SHFILES = runtests.sh
TARGETS = ${SHFILES:.sh=} ${CFILES:.c=}

default all: $(TARGETS)
	$(SUBDIRS_MAKERULE)

COMMONPREF = stress_vm_
include $(COMMONRULES)

$(COMMONTARGS) install: $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

$(COMMONPREF)install:$(TARGETS)
	$(INSTALL) -F /usr/stress/Vm '$(TARGETS)'

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)
