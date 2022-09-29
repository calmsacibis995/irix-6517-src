#!smake
#
# Makefile for xactl
#

#ident "$Revision: 1.1 $"

OBJECT_STYLE=N32

include $(ROOT)/usr/include/make/commondefs

TARGETS=xactl
CFILES=	xactl.c		\
	$(NULL)
OBJFILES=$(CFILES:.c=.o)


default: $(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
