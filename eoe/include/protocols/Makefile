#
# Makefile for bsd/protocols header file installation.
#
include $(ROOT)/usr/include/make/commondefs

PUBLIC_HFILES= bootp.h dumprestore.h routed.h rwhod.h talkd.h timed.h

HPUT= $(INSTALL) -F /usr/include/protocols -m 444

default: install

include $(COMMONRULES)

install:
	$(HPUT) $(PUBLIC_HFILES)
