#!smake
#ident	"$Revision: 1.3 $"
include	$(ROOT)/usr/include/make/commondefs

CFILES= resymlink.c
TARGETS= resymlink

LCOPTS=-fullwarn
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default: $(TARGETS)

all: default

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin ${TARGETS}
