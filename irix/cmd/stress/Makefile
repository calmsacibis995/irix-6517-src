# $Revision: 1.27 $

include $(ROOT)/usr/include/make/commondefs

SUBDIRS=lib Gfx IO Ipc Istress Misc Mmap Pigs Net Select Sproc \
	Usock Vm disks pt posix1b tape rt cmd aio Regression unixperf Tli \
	Fdpass DV

CFILES=
SHFILES=run.sh runemall.sh
TARGETS= ${SHFILES:.sh=} ${CFILES:.c=}

default all: $(TARGETS)
	$(SUBDIRS_MAKERULE)

COMMONPREF=stress_
include $(COMMONRULES)

$(COMMONTARGS) install: $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

$(COMMONPREF)install:$(TARGETS)
	$(INSTALL) -F /usr/stress '$(TARGETS)'
	$(INSTALL) -F /usr/stress README
	$(INSTALL) -F /usr/stress Makefile
	$(INSTALL) -dir /usr/stress/Report

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)
