#
# Makefile for the DLT/10i stacker control program
#

include	$(ROOT)/usr/include/make/commondefs

TARGETS	= stacker
CFILES	= stacker.c control.c
HFILES	= control.h
LCFLAGS	= -fullwarn -g
LLDOPTS	= -g
LLDLIBS = -lds
OBJECTS = $(CFILES:.c=.o)

default: $(TARGETS)

include $(COMMONRULES)

${TARGETS}:	${OBJECTS}
	${CC} ${OBJECTS} ${LDFLAGS} -o $@

stacker.o:	stacker.c control.h
control.o:	control.c control.h

install: default
	$(INSTALL) -m 755 -F /usr/sbin $(TARGETS)

