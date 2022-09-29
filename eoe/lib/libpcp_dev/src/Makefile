#!smake
#
# $Id: Makefile,v 1.6 1998/12/10 03:40:44 kenmcd Exp $

DEPTH	= .
CFILES	= access.c license.c pmauth.c AF.c logcontrol.c logconnect.c \
	  p_lrequest.c p_lcontrol.c p_lstatus.c p_control.c p_datax.c

MEMBERS = $(CFILES:S/^/\$(LIBRARY)(/g:S/.c$/.o/g:S/$/)/g)

include versiondefs
include $(ROOT)/usr/include/make/commondefs
include ../librarydefs

LCOPTS  += $(LIBRARY_COPTS)
LDIRT	+= $(LIBRARY)

default: $(LIBRARY)

include $(COMMONRULES)

$(LIBRARY): $(MEMBERS) MAKELIB
