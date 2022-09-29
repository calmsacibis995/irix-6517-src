#!smake
#
PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmstore.c
TARGETS	= pmstore
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_dev -lpcp
IDBTAG	= PCP_EOE
LDIRT	= *.pag *.dir *.log

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(TAG) 0x00010D0C pmstore
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmstore
