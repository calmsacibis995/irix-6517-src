#
# Makefile for bsd/arpa header file installation.
#
include $(ROOT)/usr/include/make/commondefs

PUBLIC_HFILES= ftp.h inet.h nameser.h telnet.h tftp.h

HPUT= $(INSTALL) -F /usr/include/arpa -m 444

default: install

include $(COMMONRULES)

install:
	$(HPUT) $(PUBLIC_HFILES)
