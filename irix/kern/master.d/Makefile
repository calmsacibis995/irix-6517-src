#!smake
#ident	"$Revision: 3.392 $"
#
# Makefile for master.d, the lboot specification directory.
#

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/master.d
.PATH:$(KPATH)

#
# compiler/loader options for master.c and final link (passed via lboot)
#
KCDEFS= -D$(GFXBOARD) -D$(CPUBOARD) -D$(SUBGR) $(KMODEOPT) -xansi
KCOPTS= -g3 -O2
# Suppress ragnarok 'this name was redefined' messages.
LWOFF=,1501
#
# NOTE: we don't want master.c to have dependencies created for it
# So ALL files compiled in this directory (of which there are currently none)
# won't get the dependency feature
MKDEPOPT=
OPTIMIZER=
# adding -x saves about 500K of symboltable space
# but unfortunately, the new symmon symbol table stuff
# isn't done yet, and -x hides all static symbols...
KLDFLAGS= -non_shared -e start

#if $(CPUARCH) == "TFP"
KLDFLAGS+=-allow_jump_at_eop
#endif

#if $(CPUBOARD) != "IP27"
KLDFLAGS+=-N
#endif

KLDFLAGS+=-woff 133

#if $(CPUBOARD) == "IP32" || $(CPUBOARD) == "IP32SIM"
MASTER_IO_PCKM=mhpckm kitchenstubs
#elif $(CPUBOARD) == "IP30" || $(CPUBOARD) == "IP27"
MASTER_IO_PCKM=ioc3_pckm kitchen_sync
#else
MASTER_IO_PCKM=pckm
#endif


# Master files, by option category
# NOTE that ALL master files are installed
MASTERS_OS= \
	asidbg \
	alp \
	ccsync \
	cfs \
	cxfs \
        cxfsarr \
	clone \
	connld \
	cpr \
	disp \
	disp_idbg \
	cell \
	dp_idbg \
	efs \
	ev_kbdms \
	fd \
	fifofs \
	gentty \
	hardlocks \
	hardlocks_statistics \
	hubspc \
	hwgfs \
	ipc \
	kbd \
	kernel \
	klog \
	klistio \
	kmem \
	kmem_debug \
	ksync \
	ksync_metered \
	ksync_named \
	ksync_statistics \
	kthread \
	mldtest \
	log \
	ldterm \
	mem \
	msg \
	namefs \
	nolocks \
	numa \
	numa_idbg \
	os \
	pgrpidbg \
	pipefs \
	pipemod \
	psema \
	prf \
	proc \
	procidbg \
	ptem \
	quotas \
	sabledsk \
	sat \
	sem \
	sesmgr \
	sessionidbg \
	sgi \
	shareII \
	sharena \
	sn0_elspec \
	sn0_elspec.dbg \
	sn0xxl_elspec.dbg \
	sn0drv \
	specfs \
	spec_idbg \
	stty_ld \
	usrdma \
	usrbus \
	vmidbg \
	xfs \
	xfsquotas \
	xfsrt \
	xfsdmapi \
	zero

MASTERS_IO= bpqueue cn dmi umfs flash graph grio imon io mcioplp mcioaud \
	sad sduart hpcplp hpc3plp ecpplp ecplp \
	epcplp usema fsctl cdsio lm $(MASTER_IO_PCKM) sysctlr \
	epcei postwait tsd xlv xlv_plex \
	xtalk xswitch xbow xbmon heart xioba \
	pci pciio pcibr pcimh pciiox pciba \
	gioio giobr \
	failover mace\
	serialio cserialio userialio tserialio \
	sio_ioc3 sio_i2c sio_dummy sio_16550 ioc3 \
	sample_xtalk sample_pciio sample_gioio nic menet io6 mio \
	kaio stream usync ioc3ei usrpci cdl \
	xiovme vmeio usrvme universe ude vmeuli \
	tpu tpuidbg tpusim \
	sflash i2c elsc clshm cl rad1pci

MASTERS_TCP=bsd ptc pts fddi uds ipfilter ip_mroute \
	if_sl if_ppp ppp_fram if_sw \
	if_hy if_et if_ec if_me if_gfe if_vfe \
	if_ec2 if_ecf if_eg2 if_fxp if_ipg if_xpi if_rns if_ee if_ef if_ep \
	if_ls if_lsp if_eg
MASTERS_NFS= nfs snfs nfsidbg
MASTERS_NFS3= nfs3
MASTERS_LOCKD= lockd
MASTERS_SVCKUDP= svckudp
MASTERS_ONC3= autofs cachefs cachefsidbg lofs
MASTERS_SCSI= wd93 scsi dksc tpsc tpscutil smfd ds wd95 scsiha \
	scip ql adp78 adphim fcadp qlfc
MASTERS_TPI= sockmod ticlts ticots ticotsord timod tirdwr \
	tpisocket tpitcp tpiudp tpirawip tpiicmp
MASTERS_TR=tr_lli if_ptr
MASTERS_MAC=mac mac_nfs3 mac_xfs mac_ipc mac_specfs mac_access mac_autofs \
	mac_efs mac_fd mac_fifo mac_pipe mac_proc mac_hwg
MASTERS_EAG=eag
MASTERS_SUSER=suser
MASTERS_ACL=acl acl_hwg acl_nfs3 acl_xfs
MASTERS_CAP=capability
MASTERS_CAP_MAC=cap_mac_access
MASTERS_SESMGR=sesmgr ip_sec
MASTERS_XFS_FETCH=xfs_attr_fetch
MASTERS_FRS=frs
MASTERS_FRU=fru
MASTERS_PART=partition
MASTERS_XPC=xpc
MASTERS_UNC=uncintf
MASTERS_IFCL=if_cl
MASTERS_IDBG=idbg dmiidbg umfsidbg graphidbg grioidbg xfsidbg xlvidbg \
	mloadidbg klidbg bsdidbg stridbg cfsidbg cxfsidbg vme_idbg \
	qlidbg fcidbg adpidbg qlfc_idbg
MASTERS_STUBS=bpqueuestubs dnstubs gfxstubs prfstubs quotasstubs idbgstubs \
	macstubs satstubs nfsstubs pipefsstubs vidstubs \
	aclstubs ng1stubs gr2stubs frsstubs \
	dmistubs umfsstubs griostubs xfsstubs xfsquotasstubs xfsdmapistubs \
	xlvplexstubs xlvstubs failoverstubs tpisocketstubs \
	efsstubs cachefsstubs cxfsstubs cxfsarrstubs \
	nlmstubs lockdstubs mgrasstubs xfsrtstubs frustubs partitionstubs \
	cachefsidbgstubs sesmgrstubs kaiostubs devicestubs crimestubs \
	cprstubs mgvstubs dfsstubs vmestubs xpcstubs if_clstubs shareIIstubs \
	postwaitstubs klistiostubs uncstubs numaidbgstubs swipestubs atmstubs \
	lofsstubs autofsstubs krpchdstubs
MASTERS_DLPI=llc2 snif xtimer
MASTERS= $(MASTERS_OS) $(MASTERS_IO) \
	$(MASTERS_TCP) $(MASTERS_NFS) $(MASTERS_NFS3) $(MASTERS_LOCKD) \
	$(MASTERS_SVCKUDP) $(MASTERS_ONC3) \
	$(MASTERS_SCSI) $(MASTERS_FRS) $(MASTERS_FRU) ${MASTERS_PART}\
	$(MASTERS_MAC) $(MASTERS_EAG) \
	$(MASTERS_TR) $(MASTERS_TPI) $(MASTERS_STUBS) \
	$(MASTERS_ACL) $(MASTERS_CAP) $(MASTERS_CAP_MAC) $(MASTERS_SESMGR) \
	$(MASTERS_SUSER) $(MASTERS_IDBG) $(MASTERS_XFS_FETCH) $(MASTERS_DLPI) \
	$(MASTERS_XPC) $(MASTERS_IFCL) $(MASTERS_UNC)

MASTERS_NOSHIP=dhardlocks sabledsk ram


SYSTEMGEN=system.gen
SYSTEMDEP=${DEPTH}/kcommondefs
DOTSMFILE=irix.sm
MISC=README
SYSTEMS=system.ship system.mr system.kdebug system.dl tirix.mr tirix.kdebug

TARGETS=$(SYSTEMS) $(MISC)

# Options passed to all versions
# SYSCONFIGDEFS can come from klocaldefs
# The -ICPUINSTDIR is for developers, the -I. is for production kernel
PRODOPTS= $(CFLAGS) $(SYSCONFIGDEFS) -DDCCOPTS="$(CDEFS) $(COPTS)" -DDLDOPTS="$(LDFLAGS)" -DCPUINSTDIRINC="-I$(ROOT)/$(CPUINSTDIR) -I." -DCOMPILATION_MODEL=$(COMPILATION_MODEL)

# Options on system.gen
SHIPOPTS= -Dship $(PRODOPTS)
MROPTS= -Dnoprobe -Dmr $(PRODOPTS)
KDEBUGOPTS= -Dkdebug -Dnoprobe -Dquotas_enabled $(PRODOPTS)
DLOPTS= -Ddiskless -Dnoprobe -dollar $(PRODOPTS)
TIRIXOPTS= -Dsat_enabled -Dmac_enabled -Dacl_enabled -Dsesmgr_enabled

INTDIRT=$(SYSTEMS)

INSTFILE= $(INSTALL) -m 444 -idb "config(update)"
INSTMAST= $(INSTFILE) -F $(MASTERDIR)
SHIP_INSTMAST= $(INSTFILE) -F $(SHIP_MASTERDIR)

#
# Rules
#
KCOMMONPREF=m
KBPREF=m
include ${DEPTH}/kcommonrules

# This default rule is always invoked in BOOTAREA
$(KCOMMONPREF)default:$(TARGETS)
	cd $(KPATH) ;\
	RAWIDB= $(INSTALL) -F $(MASTERDIR) -m 444 $(MASTERS) $(MASTERS_NOSHIP)

$(KCOMMONPREF)install: ksysinstall
	cd $(KPATH) ;\
	$(INSTMAST) $(MISC) ;\
	$(INSTMAST) $(MASTERS) ;\
	$(INSTMAST) -idb "noship.sw.kernels" $(MASTERS_NOSHIP) ;\
	cd $(KPATH) ;\
	$(SHIP_INSTMAST) $(MISC) ;\
	$(SHIP_INSTMAST) $(MASTERS) ;\
	$(SHIP_INSTMAST) -idb "noship.sw.kernels" dhardlocks

mrmtargets!
	rm -f $(TARGETS)
	if test -d $(ROOT)/$(CPUINSTDIR); then \
		cd $(ROOT)/$(CPUINSTDIR); \
		rm -fr */irix.sm; \
	fi

mclean!kclean
mclobber!kclobber
mlint!mlint

branch!$(KOPREF)default

#
# Since stuff in klocaldefs often can change the configuration make
# it easier to get it right..
#
#if exists(../klocaldefs)
$(TARGETS):../klocaldefs
#endif
