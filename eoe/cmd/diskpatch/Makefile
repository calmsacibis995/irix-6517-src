#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LLDLIBS = -lds

TARGETS=diskpatch

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
