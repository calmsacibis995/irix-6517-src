# Top-level makefile for standalone tools

DEPTH= ../
COMMONPREF=stools
include ../commondefs

SUBDIRS= convert promcvt promgen hwreg myacc

default clean clobber depend incdepend fluff rtags lint: $(_FORCE)
	@for i in $(SUBDIRS); \
	do \
		echo "=====\tcd $$i; $(MAKE) $@"; \
		( cd $$i; $(MAKE) $@ ); \
	done

include $(COMMONRULES)

tags: $(COMMONPREF)tags
	@for i in $(SUBDIRS); \
	do \
		echo "=====\tcd $$i; $(MAKE) $@"; \
		( cd $$i; $(MAKE) $@ ); \
	done

install: default 
