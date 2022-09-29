#!smake
#

include $(ROOT)/usr/include/make/commondefs

TARGETS	  = iconv

CFILES    = iconv.c

targets: $(TARGETS)

include $(COMMONRULES)

all default: $(TARGETS)

install: all default
	$(INSTALL) -F /usr/bin $(TARGETS)

#install: all default

headers exports:
