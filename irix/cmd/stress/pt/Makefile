#
# Makefile for Pthread subdir tests

include $(ROOT)/usr/include/make/commondefs

SUBDIRS = lib sanity stress
CFILES =
SHFILES = runtests.sh
TARGETS = ${SHFILES:.sh=} ${CFILES:.c=}

default all: $(TARGETS)
	$(SUBDIRS_MAKERULE)

COMMONPREF = stress_pthreads_
include $(COMMONRULES)

$(COMMONTARGS) install: $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

$(COMMONPREF)install:$(TARGETS)
	$(INSTALL) -F /usr/stress/pt '$(TARGETS)'

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)
