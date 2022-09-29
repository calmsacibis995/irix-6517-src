#!smake
#ident	"$Revision: 1.3 $"

DEPTH?=..
include $(ROOT)/usr/include/make/kcommondefs
KPATH=$(TOP)/master.d
.PATH:$(KPATH)


MASTERS= rawiso1394 dvc1394 ti_lynx ieee1394

SYSTEMGEN=ieee1394.sm
DOTSMFILE=ieee1394.sm
SYSTEMS=system.ship system.dl system.kdebug system.mr

TARGETS=$(SYSTEMS) $(OTHER)

# Options passed to all versions
# SYSCONFIGDEFS can come from klocaldefs
# The -ICPUINSTDIR is for developers, the -I. is for production kernel
PRODOPTS= $(CFLAGS) $(SYSCONFIGDEFS) -DDCCOPTS="$(CDEFS) $(COPTS)" -DDLDOPTS="$(LDFLAGS)" -DCPUINSTDIRINC="-I$(ROOT)/$(CPUINSTDIR) -I." -DCOMPILATION_MODEL=$(COMPILATION_MODEL)

# Options on system.gen
SHIPOPTS= -Dship $(PRODOPTS)
MROPTS= -Dnoprobe -Dmr $(PRODOPTS)
KDEBUGOPTS= -Dkdebug -Dnoprobe $(PRODOPTS)
DLOPTS= -Ddiskless -Dnoprobe $(PRODOPTS)

INTDIRT=$(SYSTEMS)

INSTFILE= $(INSTALL) -m 444 -idb "config(update)" 
INSTMAST= $(INSTFILE) -F $(MASTERDIR)
SHIP_INSTMAST= $(INSTFILE) -F $(SHIP_MASTERDIR)

#
# Rules
#
KCOMMONPREF=m
include $(ROOT)/usr/include/make/kcommonrules


$(KCOMMONPREF)default:$(TARGETS)
	cd $(KPATH);\
	RAWIDB= $(INSTALL) -F $(MASTERDIR) -m 444 $(MASTERS) ;

$(KCOMMONPREF)install: ksysinstall
	cd $(KPATH) ;\
	$(INSTMAST) $(MASTERS) ; \
	$(SHIP_INSTMAST) $(MASTERS) ;

mrmtargets!
	rm -f $(TARGETS)
	if test -d $(ROOT)/$(CPUINSTDIR); then \
		cd $(ROOT)/$(CPUINSTDIR); \
		rm -fr */ti_lynx.sm; \
	fi

mclean!kclean
mclobber:kclobber
mlint!mlint
