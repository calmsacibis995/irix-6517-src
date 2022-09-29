#!smake
#
# pmcd PMDA
#
# $Revision: 2.18 $

LTARGETS	= help.dir
LLDIRT		= *.log *.dir *.pag domain.h so_locations

PCP_SRC_DEPTH = ../..
include $(PCP_SRC_DEPTH)/include/commondefs
include $(PCP_SRC_DEPTH)/include/isacommondefs

IAM	= pmcd
IDBTAG	= PCP_EOE

PMDADIR	= /usr/pcp/pmdas/$(IAM)
PCPLIB	= /usr/pcp/lib

EXITOP  = `cat exitop`

install:	default help.dir
	$(INSTALL) -idb "$(IDBTAG)" -m 755 -dir $(PMDADIR)
	$(INSTALL) -f $(PMDADIR) -idb "$(IDBTAG)" -m 444 help.dir help.pag
	$(INSTALL) -f /var/pcp/pmns -m 444 -idb "$(IDBTAG) $(EXITOP)" root_pmcd
	$(I_32) $(INSTALL) -F $(PCPLIB) -idb "$(IDBTAG) $(STRIPDSO) $(MODE32)" -m 555 -src 32/pmda_$(IAM).so mips_o32.pmda_$(IAM).so
	$(I_N32) $(INSTALL) -F $(PCPLIB) -idb "$(IDBTAG) $(STRIPDSO) $(MODE32)" -m 555 -src N32/pmda_$(IAM).so mips_n32.pmda_$(IAM).so
	$(I_64) $(INSTALL) -F $(PCPLIB) -idb "$(IDBTAG) $(STRIPDSO) $(MODE64)" -m 555 -src 64/pmda_$(IAM).so mips_64.pmda_$(IAM).so
#if $(PCP_EOE_NOSHIP)
	$(INSTALL) -f $(PMDADIR) -idb "PCP_EOE_NOSHIP" -m 444 help.dir help.pag
	$(INSTALL) -f /var/pcp/pmns -m 444 -idb "PCP_EOE_NOSHIP $(EXITOP)" root_pmcd
	$(I_32) $(INSTALL) -F $(PCPLIB) -idb "PCP_EOE_NOSHIP $(STRIPDSO) $(MODE32)" -m 555 -src 32/pmda_$(IAM).so mips_o32.pmda_$(IAM).so
	$(I_N32) $(INSTALL) -F $(PCPLIB) -idb "PCP_EOE_NOSHIP $(STRIPDSO) $(MODE32)" -m 555 -src N32/pmda_$(IAM).so mips_n32.pmda_$(IAM).so
	$(I_64) $(INSTALL) -F $(PCPLIB) -idb "PCP_EOE_NOSHIP $(STRIPDSO) $(MODE64)" -m 555 -src 64/pmda_$(IAM).so mips_64.pmda_$(IAM).so
#endif

help.dir: help root_pmcd ../../buildtools/newhelp
	../../buildtools/newhelp -v 2 -n root_pmcd help
