#!smake
#
# libirixpmda.so - module to extract performance statistics from
#		the Irix kernel, as part of the Performance Co-Pilot (PCP)
#

#ident	"$Revision: 1.81 $"

include $(ROOT)/usr/include/make/releasedefs
include	$(ROOT)/usr/include/make/commondefs
include ./librarydefs

IRIX=$(RELEASE_MAJOR).$(RELEASE_MINOR)

# must set ALTLIBS or MULTIDIR in each case
#
#if $(IRIX) == "5.3"
BASELIB	= libirixpmda.a
MULTIDIR=
#elif $(IRIX) == "6.2"
BASELIB = libirixpmda.a
ALTLIBS= libirixpmda_64.a
#elif $(IRIX) == "6.3"
BASELIB = libirixpmda.a
MULTIDIR=
#elif $(IRIX) == "6.4"
ALTLIBS= libirixpmda_64.a
MULTIDIR=
#else
ALTLIBS= libirixpmda_n32.a libirixpmda_64.a
#endif

# Note this works for 6.5
#
DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

# and this is how you do it for earlier IRIX builds
#
EXP_SYMBOLS	= -exported_symbol irix_init \
			-exported_symbol dsreqflags \
			-exported_symbol __hwg2inst

FRIEND_HEADERS=impl.h pmapi.h

include	$(ROOT)/usr/include/make/librootdefs

IAM		= irix
IDBTAG		= PCP_EOE
PMDADIR		= /var/pcp/pmdas/$(IAM)
INSTDSO		= /usr/pcp/lib
INSTINC		= /usr/include/pcp
LLDLIBS		= -lpcp_pmda -lpcp -lelf -ldisk -lc
LLDDSOOPTS	= $(EXP_SYMBOLS)
LDIRT		= libirixpmda_64 libirixpmda_64.a libirixpmda_64.so \
		libirixpmda_n32 libirixpmda_n32.a libirixpmda_n32.so \
		help/help.pag help/help.dir pmda_version.h

DSONAMEOPT	= -soname libirixpmda.so

MODE32		= mach(MODE=32bit)
MODE64		= mach(MODE=64bit)

EXITOP          = `cat exitop`

default: pmda_version $(TARGETS) others

include $(LIBROOTRULES)


install!	default
	$(INSTALL) -idb "$(IDBTAG)" -m 755 -dir $(PMDADIR)
	$(INSTALL) -f /var/pcp/pmns -m 444 -idb "$(IDBTAG) $(EXITOP)" root_$(IAM)
	$(I_64) $(INSTALL) -F $(INSTDSO) -m 555 -idb "$(IDBTAG) $(STRIPDSO) $(MODE64)" -src libirixpmda_64.so mips_64.libirixpmda.so
	$(I_N32) $(INSTALL) -F $(INSTDSO) -m 555 -idb "$(IDBTAG) $(STRIPDSO) $(MODE32)" -src libirixpmda_n32.so mips_n32.libirixpmda.so
	cd help; make $@

headers exports:

others:
#ifndef SRC_PRD
	cd help; make
#endif

pmda_version:
	rm -f pmda_version.h
	echo "static char *buildversion = \"`$(TOOLROOT)/usr/sbin/mkversionnum \
	    -r "$(RELEASE_NUM)" -b "$(BUILDER)" -t "$(TREE_ID)"`\";" >pmda_version.h


