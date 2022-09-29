#!smake
# $Revision: 1.4 $

include $(ROOT)/usr/include/make/commondefs

TARGETS=spawn s1 s2 s3 s4 s5 s6 s7 runrsh runnopset supset run
IDBTAG= -idb "noship"

LCINCS=
LLDLIBS=

default all:    $(TARGETS)

include $(COMMONRULES)

install: all
	$(INSTALL) $(IDBTAG) -F /usr/stress/cmd/session '$(TARGETS)'
