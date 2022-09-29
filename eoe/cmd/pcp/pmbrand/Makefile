#!smake
#
# $Id: Makefile,v 2.15 1998/12/10 03:40:44 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmbrand.c
TARGETS	= pmbrand
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_dev -lpcp -llmsgi
IDBTAG	= PCP_EOE
LDIRT	= pmbrand_qa

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /usr/pcp/bin -u root -m 4555 -idb "$(IDBTAG) $(STRIPBIN)" pmbrand

pmbrand:	$(OBJECTS)
	rm -f $@
	$(CCF) $(OBJECTS) -o $@ $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D13 $@

pmbrand_qa:	pmbrand.c
	rm -f $@
	$(CCF) -DQA=1 pmbrand.c -o $@ $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D13 $@
