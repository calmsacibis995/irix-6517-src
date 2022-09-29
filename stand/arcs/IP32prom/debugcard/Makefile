#
# Makefile for Moosehead debugcard.
DEBUGCARD   =   .
include		$(DEBUGCARD)/mhdbcommondefs

OBJECTDIR	= $(PRODUCT:=.O)

#
# All the little branches under the tree top.
#	tools lcore diags images

SUBDIRS=\
	tools lcore diags IP32fw images

default $(COMMONTARGS): $(COMMONPREF:=default)
	$(SUBDIRS_MAKERULE)

mhdefault: $(OBJECTDIR)

include		$(DEBUGCARD)/mhdbcommonrules
