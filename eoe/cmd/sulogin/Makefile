#!smake
#ident "$Revision: 1.8 $"
include	$(ROOT)/usr/include/make/commondefs

TARGETS=sulogin
LLDOPTS=-Wl,-I,$(ROOTRLDNAME) -Wl,-woff,85
LLDLIBS=$(ROOT)/usr/$(LIB_LOCATION)/libcrypt.a

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /etc -ln ../sbin/sulogin sulogin
