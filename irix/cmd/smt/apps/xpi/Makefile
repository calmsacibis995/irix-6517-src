#!smake

# Makefile for firmware for GIO Bus FDDI boards
#
#	$Revision: 1.11 $

DEPTH=../..
include $(DEPTH)/Makedefs
SUBDIRS=tools firm
.NOTPARALLEL:

TDIR	=tools
AS	=$(TDIR)/as29i
C2FIRM	=$(TDIR)/coff2firm

# only for `make clobber`
TARGETS	=*.firm

default:subdirs

#build the tools first
firm:$(AS) $(C2FIRM)

COMMONPREF=xpi
include $(COMMONRULES)

# Nothing here is installed.
install:default

$(COMMONTARGS):$(COMMONPREF)$$@ $(_FORCE)
	@for d in $(SUBDIRS); do \
		$(ECHO) "\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

subdirs $(AS) $(C2FIRM) $(SUBDIRS):$(_FORCE)
	@for d in $(SUBDIRS); do \
		$(ECHO) "\tcd $$d; $(MAKE)"; \
		cd $$d; $(MAKE); cd ..; \
	done
