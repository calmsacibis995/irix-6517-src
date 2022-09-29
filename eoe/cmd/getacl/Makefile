#
# Makefile for getacl
#
#
include $(ROOT)/usr/include/make/commondefs

TARGETS=getacl
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LLDLIBS=

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
