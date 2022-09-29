# Makefile for sysmeter(1)
# 
#ident "$Revision: 1.7 $"

include $(ROOT)/usr/include/make/commondefs

LLDLIBS = -lgl -lrpcsvc

CFILES = sysmeter.c
TARGETS = sysmeter

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin sysmeter
