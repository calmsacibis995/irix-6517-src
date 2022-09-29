#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/csplit/RCS/Makefile,v 1.6 1992/03/26 22:10:49 wicinski Exp $"
#
# Common Definitions
#
include	$(ROOT)/usr/include/make/commondefs

TARGETS= csplit
LLDLIBS=-lw -lgen

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
