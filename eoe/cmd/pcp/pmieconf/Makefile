#!smake
#
# $Id: Makefile,v 1.2 1999/05/26 02:38:46 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs
include ./subdirs

TARGETS	= pmieconf
CFILES	= pmieconf.c rules.c io.c
LCOPTS	= -fullwarn $(PCP_INC_PATH) -I..
LCDEFS	= $(DEBUG)
LLDOPTS	= 
LLDLIBS	= 
LDIRT	= rules local Makedepend pmieconf pcp_eoe pcp

IDBTAG	= PMIE
IDBBASE	= PCP_BASE
EOELIST	= rules:../pmdas/espping
PCPLIST	= ../pmdas/cisco:../pmdas/environ:../pmdas/shping
ESPDIR	= /var/esp/init/eventlist

default:	makefiles $(TARGETS) pmie_email


COMMONPREF = pmieconf
include $(COMMONRULES)

install:	default rules pcp pcp_eoe
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmieconf
	$(INSTALL) -F /usr/pcp/lib -m 555 -idb "$(IDBTAG)" pmie_email
	$(INSTALL) -F $(ESPDIR) -m 444 -idb "$(IDBTAG) config(suggest)" pcp_eoe
#if $(ISM_NAME) == "pcp"
	$(INSTALL) -F $(ESPDIR) -m 444 -idb "$(IDBBASE) config(suggest)" pcp
#endif
	$(SUBDIRS_MAKERULE)

pmieconf:	$(OBJECTS)
	rm -f $@
	$(CCF) $(OBJECTS) -o $@ $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D13 $@


rules:	default
	@rm -fr rules local; mkdir rules; exit 0
	$(SUBDIRS_MAKERULE)
	../buildtools/pmieconf -F -r rules -f local

pcp_eoe:	rules
	@rm -f $@ local
	../buildtools/pmieconf -f local -r $(EOELIST) list all esp_type | ../buildtools/esp_events "in pcp_eoe" > $@

pcp:	rules
	@rm -f $@ local
#if $(ISM_NAME) == "pcp"
	../buildtools/pmieconf -f local -r $(PCPLIST) list all esp_type | ../buildtools/esp_events "in pcp" > $@
#endif

clean:	makefiles
	$(SUBDIRS_MAKERULE)

clobber:	makefiles
	$(SUBDIRS_MAKERULE)
	rm -rf $(DIRT)
	for d in $(SUBDIRS); \
	do \
	    test -d $$d \
	    && test -l $$d/Makefile \
	    && rm -f $$d/Makefile; \
	done; exit 0

makefiles:
	@for d in $(SUBDIRS); \
	do \
	    test -d $$d \
	    && test ! -f $$d/Makefile && test ! -l $$d/Makefile \
	    && ln -s ../Makefile.rules $$d/Makefile; \
        done; exit 0
