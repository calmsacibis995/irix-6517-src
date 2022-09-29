#!smake
#
PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmval.c
TARGETS	= pmval
LCOPTS	= -fullwarn $(PCP_INC_PATH) -dollar -I$(ROOT)/usr/include/idl/c
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_dev -lpcp -lm
IDBTAG	= PCP_EOE_MON
LDIRT	=

default:	$(TARGETS)

include $(COMMONRULES)

install:	$(TARGETS)
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmval

pmval:	$(OBJECTS)
	rm -f $@
	$(CCF) -o $@ $(OBJECTS) $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D0B $@
