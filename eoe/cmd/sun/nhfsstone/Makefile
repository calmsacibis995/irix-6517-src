# @(#)Makefile 1.6 89/07/07 Legato Systems, Inc.
#
# Makefile to build nhfsstone
#
include $(ROOT)/usr/include/make/commondefs

CVERSION=-cckr

SHAR_FILES= README DISCLAIMER Makefile nhfsstone.1 nhfsstone.c \
	    nhfsrun nhfsgraph nhfsnums

LCOPTS=-D_BSD_SIGNALS -D_BSD_TIME
LLDLIBS= -lbsd -lelf

CFILES= nhfsstone.c

INST= $(INSTALL) -F /usr/local/bin

LDIRT= nhfsstone.shar lint.out
TARGETS= nhfsstone

default: $(TARGETS)

include $(COMMONRULES)

nhfsstone.shar: $(SHAR_FILES)
	rm -f nhfsstone.shar
	shar $(SHAR_FILES) > nhfsstone.shar

lint: lint.out

lint.out: nhfsstone.c
	lint nhfsstone.c | tee lint.out

install: default
	$(INST) -g sys -m 2755 nhfsstone
	$(INST) nhfsrun nhfsgraph nhfsnums
	$(INSTALL) -F /usr/share/catman/local/man1 nhfsstone.1
