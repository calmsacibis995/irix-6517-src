#!smake
#
# Makefile for X test directory
#

SUBDIRS = xcrash xstress

all:
	@set -x; for i in ${SUBDIRS}; do \
	    (cd $$i && $(MAKE) all); \
	done

install:
	@set -x; for i in ${SUBDIRS}; do \
	    (cd $$i && $(MAKE) install); \
	done
#	cp /usr/bin/X11/x11perf $(ROOT)/usr/diags/regression/X; \

clean:
	@set -x; for i in ${SUBDIRS}; do \
	    (cd $$i && $(MAKE) clean); \
	done

clobber:
	@set -x; for i in ${SUBDIRS}; do \
	    (cd $$i && $(MAKE) clobber); \
	done
