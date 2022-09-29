#ident	"$Revision: 1.1 $"

include	$(ROOT)/usr/include/make/commondefs

CFILES	=minrss.c
TARGETS	=$(CFILES:.c=)

default:$(TARGETS)

include ${COMMONRULES}

install: default
	$(INSTALL) -F /usr/sbin "$(TARGETS)"
