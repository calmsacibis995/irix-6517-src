#
#	Makefile for the gettapedev commmand used by the RESTORE shell script
#

include $(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

TARGETS	= gettapedev

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)
