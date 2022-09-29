#
# Makefile for whodo(1).
#
#"$Revision: 1.15 $"

include	${ROOT}/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

CFILES= whodo.c
TARGETS= ${CFILES:.c=}

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -F /sbin ${TARGETS}
	$(INSTALL) -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)

