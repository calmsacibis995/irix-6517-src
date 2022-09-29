#ident "$Revision: 1.10 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=nvram sgikopt
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(COMMONRULES)

sgikopt: nvram
	ln -f nvram sgikopt

install: default
	${INSTALL} -F /sbin nvram
	${INSTALL} -F /etc -lns ../sbin/nvram nvram
	$(INSTALL) -F /usr/sbin -lns ../../sbin/nvram sgikopt
