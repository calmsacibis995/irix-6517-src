#ident	"$Revision: 1.22 $"

include	$(ROOT)/usr/include/make/commondefs

CFILES	=killall.c
TARGETS	=$(CFILES:.c=)
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LCOPTS=-fullwarn

default:$(TARGETS)

include ${COMMONRULES}

install: default
	$(INSTALL) -u root -g sys -m 755 -F /sbin "$(TARGETS)"
	$(INSTALL) -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)
