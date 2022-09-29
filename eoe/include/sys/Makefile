#
# Makefile for bsd/sys header file installation.
#
include $(ROOT)/usr/include/make/commondefs

PUBLIC_HFILES= msgbuf.h ttychars.h

HPUT= $(INSTALL) -F /usr/include/sys -m 444

default: install

include $(COMMONRULES)

install:
	$(HPUT) $(PUBLIC_HFILES)
