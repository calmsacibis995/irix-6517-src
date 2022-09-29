#!smake
#
# proc PMDA
#
# $Revision: 2.14 $

LTARGETS	= help.dir
LLDIRT		= *.log *.dir *.pag

PCP_SRC_DEPTH = ../..
include $(PCP_SRC_DEPTH)/include/commondefs
include $(PCP_SRC_DEPTH)/include/isacommondefs


IAM	= proc
IDBTAG	= PCP_EOE

PMDADIR	= /usr/pcp/pmdas/$(IAM)
PCPLIB	= /usr/pcp/lib

EXITOP  = `cat exitop`

install:	default
	$(INSTALL) -F $(PMDADIR) -idb "$(IDBTAG)" -m 444 help.pag help.dir
	$(INSTALL) -f /var/pcp/pmns -m 444 -idb "$(IDBTAG) $(EXITOP)" root_proc
	$(I_32) $(INSTALL) -F $(PCPLIB) -idb "$(IDBTAG) $(STRIPDSO) $(MODE32)" -m 555 -src 32/pmda_$(IAM).so mips_o32.pmda_$(IAM).so
	$(I_N32) $(INSTALL) -F $(PCPLIB) -idb "$(IDBTAG) $(STRIPDSO) $(MODE32)" -m 555 -src N32/pmda_$(IAM).so mips_n32.pmda_$(IAM).so
	$(I_64) $(INSTALL) -F $(PCPLIB) -idb "$(IDBTAG) $(STRIPDSO) $(MODE64)" -m 555 -src 64/pmda_$(IAM).so mips_64.pmda_$(IAM).so


help.dir: help root_proc ../../buildtools/newhelp
	../../buildtools/newhelp -v 2 -n root_proc help
