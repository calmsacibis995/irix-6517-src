#!smake
#
# $Id: Makefile,v 1.2 1999/05/11 00:28:03 kenmcd Exp $
#

PCP_SRC_DEPTH = ../..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

TARGETS	= pmda$(IAM)
CFILES	= shping.c pmda.c
OTHERS	= README root help pmns domain.h sample.conf

LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_pmda -lpcp -lgen

IAM	= shping
DOMAIN	= SHPING
IDBTAG	= PMDA_$(DOMAIN)
IDBBASE	= PCP_BASE
IDBMON	= PCP_MON
PMDADIR = /var/pcp/pmdas/$(IAM)
PMCHART	= /var/pcp/config/pmchart
PMIE	= /var/pcp/config/pmieconf/$(IAM)
LDIRT	= *.log *.dir *.pag so_locations a.out domain.h

default: domain.h $(TARGETS) check_help_src

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/pcp/lib -idb "$(IDBTAG)" -lns ../../..$(PMDADIR)/pmda$(IAM) pmda$(IAM)
	$(INSTALL) -idb '$(IDBTAG) removeop("rm -f $$rbase$(PMDADIR)/help.*")' -m 755 -dir $(PMDADIR)
	$(INSTALL) -F /usr/pcp/pmdas -idb "$(IDBTAG)" -lns ../../..$(PMDADIR) $(IAM)
	$(INSTALL) -f $(PMDADIR) -idb "$(IDBTAG) $(STRIPBIN)" -m 555 pmda$(IAM)
	$(INSTALL) -f $(PMDADIR) -idb "$(IDBTAG)" -m 555 Install Remove
	$(INSTALL) -f $(PMDADIR) -idb "$(IDBTAG)" -m 444 $(OTHERS)
	$(INSTALL) -f $(PMDADIR) -idb "$(IDBTAG)" -m 444 -src Makefile.install Makefile
	$(INSTALL) -F $(PMIE) -idb "$(IDBBASE)" -m 444 -src shping.status.pmie status
	$(INSTALL) -F $(PMIE) -idb "$(IDBBASE)" -m 444 -src shping.response.pmie response
	$(INSTALL) -F $(PMCHART) -idb "$(IDBMON)" -m 444 -src shping.CPUTime.pmchart shping.CPUTime
	$(INSTALL) -F $(PMCHART) -idb "$(IDBMON)" -m 444 -src shping.RealTime.pmchart shping.RealTime

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
