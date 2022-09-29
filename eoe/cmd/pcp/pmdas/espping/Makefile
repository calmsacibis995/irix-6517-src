#!smake
#
# $Id: Makefile,v 1.2 1999/05/26 02:38:46 kenmcd Exp $
#

PCP_SRC_DEPTH = ../..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

COPIES		= espping.c pmda.c espping.h \
		  Makefile.install README Remove root pmns help \
		  espping.RealTime.pmchart espping.CPUTime.pmchart

TARGETS	= pmda$(IAM)
CFILES	= espping.c pmda.c
OTHERS	= README root help pmns domain.h sample.conf sample.map template.conf

LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_pmda -lpcp

IAM	= espping
DOMAIN	= ESPPING
IDBTAG	= PMDA_$(DOMAIN)
IDBEOE	= PMIE
IDBMON	= PCP_EOE_MON
PMDADIR	= /var/pcp/pmdas/$(IAM)
PMCHART	= /var/pcp/config/pmchart
PMIE	= /var/pcp/config/pmieconf/$(IAM)
LDIRT	= *.log *.dir *.pag so_locations a.out domain.h $(COPIES)

default: $(COPIES) domain.h $(TARGETS) check_help_src

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/pcp/lib -idb "$(IDBTAG)" -lns ../../..$(PMDADIR)/pmda$(IAM) pmda$(IAM)
	$(INSTALL) -idb '$(IDBTAG) removeop("rm -f $$rbase$(PMDADIR)/help.*")' -m 755 -dir $(PMDADIR)
	$(INSTALL) -F /usr/pcp/pmdas -idb "$(IDBTAG)" -lns ../../..$(PMDADIR) $(IAM)
	$(INSTALL) -f $(PMDADIR) -idb "$(IDBTAG) $(STRIPBIN)" -m 555 pmda$(IAM)
	$(INSTALL) -f $(PMDADIR) -idb "$(IDBTAG)" -m 555 Install Remove Configure
	$(INSTALL) -f $(PMDADIR) -idb "$(IDBTAG)" -m 444 $(OTHERS)
	$(INSTALL) -f $(PMDADIR) -idb "$(IDBTAG)" -m 444 -src Makefile.install Makefile
	$(INSTALL) -F $(PMCHART) -idb "$(IDBMON)" -m 444 -src espping.CPUTime.pmchart espping.CPUTime
	$(INSTALL) -F $(PMCHART) -idb "$(IDBMON)" -m 444 -src espping.RealTime.pmchart espping.RealTime
	$(INSTALL) -F $(PMIE) -idb "$(IDBEOE)" -m 444 -src espping.status.pmie status
	$(INSTALL) -F $(PMIE) -idb "$(IDBEOE)" -m 444 -src espping.response.pmie response

pmda$(IAM):	domain.h $(OBJECTS)
	rm -f $@
	$(CCF) -o $@ $(OBJECTS) $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D01 $@

check_help_src:	domain.h help root pmns
	PCP_SRC_DEPTH=$(PCP_SRC_DEPTH) $(PCP_SRC_DEPTH)/buildtools/check_help_src help root

.NOTPARALLEL:
.ORDER:	domain.h $(OBJECTS)

domain.h:	../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from /var/pcp/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	nawk <../../pmns/stdpmid >>domain.h '/#define[ 	][ 	]*$(DOMAIN)[ 	]/ { print "#define $(DOMAIN) " $$3 }'

espping.c: ../shping/shping.c
	rm -f $@
	sed -e 's/shping/espping/g' -e 's/SHPING/ESPPING/g' <../shping/shping.c >$@

pmda.c: ../shping/pmda.c
	rm -f $@
	sed -e 's/shping/espping/g' -e 's/SHPING/ESPPING/g' <../shping/pmda.c >$@

espping.h: ../shping/shping.h
	rm -f $@
	sed -e 's/shping/espping/g' -e 's/SHPING/ESPPING/g' <../shping/shping.h >$@

root:	../shping/root
	rm -f $@
	sed -e 's/shping/espping/g' -e 's/SHPING/ESPPING/g' <../shping/root >$@

help:	../shping/help
	rm -f $@
	sed -e 's/shping/espping/g' -e 's/SHPING/ESPPING/g' <../shping/help >$@

pmns:	../shping/pmns
	rm -f $@
	sed -e 's/shping/espping/g' -e 's/SHPING/ESPPING/g' <../shping/pmns >$@

Makefile.install:	../shping/Makefile.install
	rm -f $@
	sed -e 's/shping/espping/g' -e 's/SHPING/ESPPING/g' <../shping/Makefile.install >$@

README:	../shping/README
	rm -f $@
	sed -e 's/shping/espping/g' -e 's/SHPING/ESPPING/g' <../shping/README >$@

Remove:	../shping/Remove
	rm -f $@
	sed -e 's/shping/espping/g' -e 's/SHPING/ESPPING/g' <../shping/Remove >$@

espping.CPUTime.pmchart:	../shping/shping.CPUTime.pmchart
	rm -f $@
	sed -e 's/shping/espping/g' <../shping/shping.CPUTime.pmchart >$@

espping.RealTime.pmchart:	../shping/shping.RealTime.pmchart
	rm -f $@
	sed -e 's/shping/espping/g' <../shping/shping.RealTime.pmchart >$@

