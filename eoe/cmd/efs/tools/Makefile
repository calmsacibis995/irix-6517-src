#!smake
#ident $Revision: 1.17 $
include ../commondefs

CFILES= findblk.c
TARGETS= ${CFILES:.c=}

LCINCS = -I../lib
LLDOPTS = -L../lib -Wl,-I,$(ROOTRLDNAME)
LLDLIBS = -lefs

include ../commonrules

# many get made, but few are installed
install: all
	$(INSTALL) -F /sbin makeproto
	$(INSTALL) -F /etc -lns ../sbin/makeproto makeproto
	${INSTALL} -F /sbin ${TARGETS}
