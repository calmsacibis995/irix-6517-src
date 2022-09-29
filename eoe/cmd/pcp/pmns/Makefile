#!smake
#
# $Id: Makefile,v 2.46 1999/05/27 18:55:08 kenmcd Exp $

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmnsmerge.c pmnsutil.c
TARGETS	= pmnsmerge pmnsdel
SCRIPTS = pmnsadd

STD_IRIX	= stdpmid.irix
STD_OTHER	= stdpmid.add-on stdpmid.local stdpmid.pcp
STD_QA		= stdpmid.qa

LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	=  $(DEBUG) $(VERSOPT)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp

IDBTAG_EOE		= PCP_EOE
IDBTAG_BASE		= PCP_BASE
IDBTAG_ROOT		= PCP_EOE_ROOT

LDIRT	= *.log *.pmns stdpmid
EXITOP	= `cat exitop`

default: $(SCRIPTS) $(TARGETS) Makefile.install Rebuild Xlate-irix-names stdpmid

include $(COMMONRULES)

install:	default
#if $(ISM_NAME) == "pcp"
#
# pcp.sw.base
#
	$(INSTALL) -f /var/pcp/pmns -m 444 -idb "$(IDBTAG_BASE) $(EXITOP)" $(STD_OTHER) $(STD_QA)
#endif
#if $(BEFORE_IRIX6_5) || $(ISM_NAME) == "irix"
#
# pcp_eoe.sw.eoe
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG_EOE) $(STRIPBIN)" pmnsdel pmnsadd
	$(INSTALL) -idb '$(IDBTAG_EOE) removeop("rm -f $$rbase/var/pcp/pmns/*")' -m 755 -dir /var/pcp/pmns
	$(INSTALL) -f /var/pcp/pmns -m 444 -idb "$(IDBTAG_EOE) config(update)" -src Makefile.install Makefile
	$(INSTALL) -f /var/pcp/pmns -m 555 -idb "$(IDBTAG_EOE) $(EXITOP)" Rebuild
	$(INSTALL) -f /var/pcp/pmns -m 555 -idb "$(IDBTAG_EOE)" Xlate-irix-names
	$(INSTALL) -f /var/pcp/pmns -m 555 -idb "$(IDBTAG_EOE)" Make.stdpmid
	$(INSTALL) -f /var/pcp/pmns -m 444 -idb "$(IDBTAG_EOE) config(noupdate)" -src root_null root
	$(INSTALL) -F /usr/pcp/lib -m 555 -idb "$(IDBTAG_EOE)" pmnsproc.sh
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG_EOE) $(STRIPBIN)" pmnsmerge
	$(INSTALL) -f /var/pcp/pmns -m 444 -idb "$(IDBTAG_EOE)" $(STD_IRIX)
#
# pcp_eoe_root.sw.root
	$(INSTALL) -f /var/pcp/pmns -m 444 -idb "$(IDBTAG_ROOT) config(suggest)" stdpmid
	$(INSTALL) -f /var/pcp/pmns -m 444 -idb "$(IDBTAG_ROOT) config(noupdate)" -src root_null root
#endif
#if $(PCP_EOE_NOSHIP)
#
# pcp_eoe_noship
	$(INSTALL) -f /var/pcp/pmns -m 555 -idb "PCP_EOE_NOSHIP $(EXITOP)" Rebuild
	$(INSTALL) -f /var/pcp/pmns -m 555 -idb "PCP_EOE_NOSHIP" Xlate-irix-names
#endif

pmnsmerge:	pmnsmerge.o pmnsutil.o
	rm -f $@
	$(CCF) pmnsmerge.o pmnsutil.o $(LDFLAGS) -o $@
	$(TAG) 0x00010D13 $@

pmnsdel:	pmnsdel.o pmnsutil.o
	rm -f $@
	$(CCF) pmnsdel.o pmnsutil.o $(LDFLAGS) -o $@
	$(TAG) 0x00010D13 $@

stdpmid:	$(STD_IRIX) $(STD_OTHER) $(STD_QA)
	make -f Makefile.install stdpmid
