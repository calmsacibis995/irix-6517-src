#!smake
#
# Makefile to make the kernel.
#
#ident	"$Revision: 3.178 $"

DEPTH=.
IMTOP=true

# These may be reset by klocaldefs
DEBUGUNIXS=unix.kdebug
UNIXS=$(DEBUGUNIXS)
BOOT_FILES_TO_BE_REMOVED=Set_this_in_klocaldefs_to_simulate_autoconfig

#
# Used when creating only mini-root versions of kernel
# Use "make mr"
#
UNIX_MR=unix.mr


# has to be here, or can't override the UNIXS define; still
# sinclude'd from kcommondefs also.
sinclude $(DEPTH)/klocaldefs

include $(DEPTH)/kcommondefs
KPATH=$(TOP)
.PATH:$(KPATH) $(BOOTAREA)
include $(RELEASEDEFS)
.NOTPARALLEL:

#if defined(PRODUCT) && !empty(PRODUCT)
NOPROD=ignore
PROD=
#else
NOPROD=
PROD=prod
#endif

KDIRT=bwhen.h
KLIBS=$(BOOTAREA)/ml.a $(BOOTAREA)/btool_lib.o $(BOOTAREA)/prsgi.o

HEADERS_SUBDIRS=bsd fs os sys ksys

# System files are composed in master.d/Makefile
SYSTEM_MR=$(CPUINSTDIR)/system.mr
SYSTEM_KDEBUG=$(CPUINSTDIR)/system.kdebug
TIRIX_MR=$(CPUINSTDIR)/tirix.mr
TIRIX_KDEBUG=$(CPUINSTDIR)/tirix.kdebug

# Exported files from this dir
NOSHIP_PUBLIC_EXPORTS=kcommondefs kcommonrules


#if defined(CELL)
SUBDIRS=cell/kernidl cell
LINTSUBDIRS=cell
#else
SUBDIRS=
LINTSUBDIRS=
#endif

SUBDIRS+=os sgi fs io bsd ml master.d stubs mtune btool
LINTSUBDIRS+=os sgi fs io bsd ml

TARGETS=$(UNIXS)
.PRECIOUS: $(UNIXS)

#
# Rules
#
# If PRODUCT is set then the $(PROD) rules are used else
# the $(NOPROD) rules are used.
# We set KOPREF to override the rules in kcommonrules
#
KOPREF=kk
KBPREF=kk
include $(DEPTH)/kcommonrules

#
# Rules when PRODUCT is set
#
$(PROD)install!$(BOOTAREA) bootheaders headers installsubdirs $(TARGETS) links

$(PROD)links!
	cd $(BOOTAREA);\
	$(INSTALL) -F /var/sysgen  -idb "mach($(CPUBOARD)$(SUBPRODUCT) MODE=$(MODETAG))" \
 -idb \
'postop("rm -f $$rbase/var/sysgen/boot; if [ -l $$rbase/var ]; \
then \
	ln -s ../../../usr/cpu/sysgen/$(CPUBOARD)boot \
		$$rbase/var/sysgen/boot ;\
else \
        ln -s ../../usr/cpu/sysgen/$(CPUBOARD)boot \
		$$rbase/var/sysgen/boot ;\
fi")' -lns /usr/cpu/sysgen/$(CPUBOARD)boot boot ;\
	cd ..

$(PROD)default!$(BOOTAREA) $(SUBDIRS) $(TARGETS)

$(PROD)subdirs!.RECURSIVE
$(PROD)subdirs!
	@RULE=default; export $$RULE; $(KSUBDIRS_MAKERULE)

$(PROD)headers!doheadersubdirs
	$(INSTALL) -idb "noship" -m 444  \
		-F /usr/include/make ${NOSHIP_PUBLIC_EXPORTS} ;\

doheadersubdirs!.RECURSIVE
doheadersubdirs!
	@RULE=headers; export $$RULE; \
	for d in $(HEADERS_SUBDIRS); do $(SUBDIR_MAKERULE); done

$(PROD)bootheaders!
	@RAWIDB= ; export RAWIDB; \
	RULE=headers; export $$RULE; \
	for d in $(HEADERS_SUBDIRS); do $(SUBDIR_MAKERULE); done


$(PROD)version!$(BOOTAREA)
	@cd ${BOOTAREA} ;\
	sh ${KPATH}/master.d/bwhen.sh $(RELEASE) ;\
	echo "\tInstalling bwhen.h" ;\
	${KLINKINSTALL} bwhen.h ;\
	${KDEVINSTALL} bwhen.h ;\
	${KINSTALL} bwhen.h

$(PROD)clobber!doclobbersubdirs dormtargets
	-rm -fr $(BOOTAREA)

doclobbersubdirs!.RECURSIVE
doclobbersubdirs!
	@RULE=clobber; export $$RULE; $(KSUBDIRS_MAKERULE)

$(PROD)clean!docleansubdirs

docleansubdirs!.RECURSIVE
docleansubdirs!
	@RULE=clean; export $$RULE; $(KSUBDIRS_MAKERULE)

$(PROD)rmtargets!dormtargetssubdirs dormtargets

dormtargets!
	if [ -d $(BOOTAREA) ] ; then \
		cd $(BOOTAREA); \
		rm -f bwhen.h kernel.o $(TARGETS) ;\
	fi
	if [ -d $(ROOT)/$(CPUINSTDIR) ] ; then \
		cd $(ROOT)/$(CPUINSTDIR); \
		rm -f bwhen.h kernel.o edt.list master.c $(TARGETS) ;\
	fi

dormtargetssubdirs!.RECURSIVE
dormtargetssubdirs!
	@RULE=rmtargets; export $$RULE; $(KSUBDIRS_MAKERULE)

#
# Rules when PRODUCT is not set
#
$(NOPROD)default $(NOPROD)version $(NOPROD)subdirs $(NOPROD)clean $(NOPROD)rmtargets!
	@for p in $(EVERYPRODUCT) ; do \
		PRODUCT=$$p; export PRODUCT ;\
		echo "PRODUCT=$$p $(MAKE) $(@:S/${NOPROD}/$(PROD)/)" ;\
		$(MAKE) $(@:S/${NOPROD}/$(PROD)/) ;\
	done

$(NOPROD)install!
	@for p in $(EVERYPRODUCT) ; do \
		PRODUCT=$$p; export PRODUCT ;\
		echo "PRODUCT=$$p $(MAKE) $(@:S/${NOPROD}/$(PROD)/)" ;\
		$(MAKE) $(@:S/${NOPROD}/$(PROD)/) ;\
	done
	$(INSTALL) -F /var/sysgen \
 -idb \
'postop("rm -f $$rbase/var/sysgen/root; if [ -l $$rbase/var ]; \
then \
	ln -s ../../../usr/cpu/sysgen/root $$rbase/var/sysgen/root ;\
else \
	ln -s ../../usr/cpu/sysgen/root $$rbase/var/sysgen/root ;\
fi")' -lns /usr/cpu/sysgen/root root
	$(INSTALL) -dir $(MASTERDIR)
	$(INSTALL) -dir $(SHIP_MASTERDIR)
	$(INSTALL) -idb 'exitop("if [ $${mr} = true -a -l $$rbase/usr/sysgen ]; then rm -f $$rbase/usr/sysgen; fi; if [ $${mr} = true -a -d $$rbase/usr/sysgen ]; then mv $$rbase/usr/sysgen $$rbase/usr/sysgen.O; fi")' -dir /var/sysgen 
	$(INSTALL) -m 444 -F $(SHIP_SYSGENDIR) -src io/Makefile.static \
		Makefile.kernio
	$(INSTALL) -m 444 -F $(SHIP_SYSGENDIR) -src io/Makefile.loadable \
		Makefile.kernloadio

$(NOPROD)clobber!wipeallprodbootarea
	@for p in $(EVERYPRODUCT) ; do \
		PRODUCT=$$p; export PRODUCT ;\
		echo "PRODUCT=$$p $(MAKE) $(@:S/${NOPROD}/$(PROD)/)" ;\
		$(MAKE) $(@:S/${NOPROD}/$(PROD)/) ;\
	done

wipeallprodbootarea!
	-rm -fr *bootarea

# Currently headers are not machine specific
$(NOPROD)headers!$(PROD)headers

#
# Allow smake -n to work well
#
.RECURSIVE: $(NOPROD)install $(NOPROD)default $(NOPROD)version \
	$(NOPROD)subdirs $(NOPROD)clobber $(NOPROD)clean $(NOPROD)rmtargets

installsubdirs!.RECURSIVE
installsubdirs!
	@RULE=install; export $$RULE; $(KSUBDIRS_MAKERULE)

#
# NOTE: For TFP kernels the order of the "-u" entries is extremely important.
#	The modules utlbmiss.o and tfpcacheops.o have page alignment
#	requirements which "ld" can only handle if these are the first
#	modules loaded.
#
# NOTE2: these undefs are important for loading files within ml itself.
#	To add undefines to force certain files/routines to be brought
#	in (such as various functions in 'os') add references to them
#	in ml/csu.s
#

#if defined(CPUARCH) && ($(CPUARCH) == "R4000" || $(CPUARCH) == "R10000")
ARCHDEP_UNDEFS=-u VEC_fpe -u ecc_exception
#endif

#if defined(CPUARCH) && $(CPUARCH) == "TFP"
ARCHDEP_UNDEFS=
#endif

#if defined(KERNTRACE)
ARCHDEP_UNDEFS+=-u _trentry
#endif
#if defined(CELL) && defined(SUBGR) && ($(SUBGR)=="IP19" || $(SUBGR)=="IP25")
ARCHDEP_UNDEFS+=-u part_init -u xpc_init
#endif

$(BOOTAREA)/kernel.o: $(KLIBS)
	cd $(BOOTAREA) ;\
	$(LDF) -r -o kernel.o \
	-u utlbmiss -u config_cache \
	-u first_address \
	-u VEC_tlbmiss -u VEC_tlbmod -u VEC_addrerr -u VEC_ibe -u VEC_dbe \
	-u VEC_trap -u VEC_cpfault -u VEC_int -u VEC_unexp -u VEC_nofault \
	-u VEC_breakpoint -u _physmem_start \
	${ARCHDEP_UNDEFS} \
	-u resume -u start \
	-u reviderror -u us_delay -u pio_badaddr_val \
	-u dma_mapinit -u softfp_adderr \
	-u _hook_exceptions -u getrand -u set_nvram \
	-u arcs_write -u dumptlb -u invaltlb -u atomicAddInt \
	-u __ll_div -u __ll_mul -u __ll_bit_extract -u __ll_bit_insert \
	-u ust_init -u in_cksum_sub \
	-u clean_dcache -u clean_icache \
	-u getsysid \
	-u hwperf_init \
	-u bcopy \
	-u clean_icache \
	-u cpu_isvalid \
	-u bitlock_set \
	-u iograph_early_init \
	-u pcibr_error_init \
	-u pio_geth \
	-u add_cpuboard \
	-u sendgroupintr \
	-u read_sn0_refcntrs \
	-u tlbdropin \
	-u part_init \
	-u xpc_init \
	-u sysctlr_init \
	-u otherprda \
        -u tlbwired \
	$(KLIBS:T) \
	-u hubdev_register 

#TBD: Remove pio_geth, above, when ml/*/iobus.c goes away.

installkernel! $(BOOTAREA)/kernel.o
	cd $(BOOTAREA) ; ${KLINKINSTALL} kernel.o
	cd $(BOOTAREA) ; ${KDEVINSTALL} kernel.o
	cd $(BOOTAREA) ; ${KINSTALL} kernel.o

#
# Always re-lboot
#
nounix!$(PROD)version installkernel

$(PROD)unix_mr!
	$(MAKE) UNIXS=$(UNIX_MR) default

$(NOPROD)unix_mr!
	@for p in $(EVERYPRODUCT) ; do \
		PRODUCT=$$p; export PRODUCT ;\
		echo "PRODUCT=$$p $(MAKE) $(@:S/${NOPROD}/$(PROD)/)" ;\
		$(MAKE) $(@:S/${NOPROD}/$(PROD)/) ;\
	done

unix.kdebug!$(PROD)version installkernel
	-(cd $(BOOTAREA); rm -f $(BOOT_FILES_TO_BE_REMOVED))
	$(LBOOT) $(LBOOTOPTS) -m $(ROOT)/$(MASTERDIR) -b $(ROOT)/$(CPUINSTDIR) \
		-u $(ROOT)/$(CPUINSTDIR)/$@ -s $(ROOT)/$(SYSTEM_KDEBUG) \
		-c $(STUNEFILE) -n $(ROOT)/$(MTUNEDIR)
	$(SETSYM) $(ROOT)$(CPUINSTDIR)/$@
trix.kdebug!$(PROD)version installkernel
	$(LBOOT) $(LBOOTOPTS) -m $(ROOT)/$(MASTERDIR) -b $(ROOT)/$(CPUINSTDIR) \
		-u $(ROOT)/$(CPUINSTDIR)/$@ -s $(ROOT)/$(TIRIX_KDEBUG) \
		-c $(STUNEFILE) -n $(ROOT)/$(MTUNEDIR)
	$(SETSYM) $(ROOT)$(CPUINSTDIR)/$@
unix.mr!$(PROD)version installkernel
	$(LBOOT) $(LBOOTOPTS) -m $(ROOT)/$(MASTERDIR) -b $(ROOT)/$(CPUINSTDIR) \
		-u $(ROOT)/$(CPUINSTDIR)/$@ -s $(ROOT)/$(SYSTEM_MR) \
		-c $(STUNEFILE) -n $(ROOT)/$(MTUNEDIR)

trix.mr!$(PROD)version installkernel
	$(LBOOT) $(LBOOTOPTS) -m $(ROOT)/$(MASTERDIR) -b $(ROOT)/$(CPUINSTDIR) \
		-u $(ROOT)/$(CPUINSTDIR)/$@ -s $(ROOT)/$(TIRIX_MR) \
		-c $(STUNEFILE) -n $(ROOT)/$(MTUNEDIR)

#
# Branch coverage tool
#
# btool is kind of sensitive, so its important to follow some guidelines.
#
# If you have a totally clean workarea - simply type:
#	make branch
# At the top level - a unix.kbtool will emerge sooner or later
#
# To instrument a particular directory (and any other directories beneath it
# simply 'cd' to the directory and:
#	make branch
#
# If you have a clean work area this will re-compile everything with
# branch tool. If you have already built the directory w/o using
# the branch tool, you might want to pass in the -u option to force
# everything to recompile.
#
# Then, to make a kernel, go back to the top level and type:
#	make unix.kbtool
#
# Once you have done this once you can add file/directories by repeating the
# procedure above. You can also re-instrument a directory/file that has already
# been instrumented HOWEVER each time a file is instrumented, the count of
# branches (kept in $(BOOTAREA)/btool_map.count) is incremented - NO
# numbers are reused, and the maximum number is used to dimension a
# static array in the kernel. This means that after a while you'll
# have a HUGE array that isn't used very much.
# To reset this either:
#	make btoolstart
# And then re-instrument ALL directories/files that you had instrumented
# OR
#	make clobber (or rm -fr IPxxbootarea does the trick)
# And start all over.
#
# Its imperative to re-compile btool_lib.c whenever anything else
# changes so we always want to decend to btool/ to check
#
$(PROD)branch!$(BOOTAREA) btoolsubdirs unix.kbtool

#
# WARNING - if you invoke this rule you had better make sure that
# ANY directories/files that had previously been compiled with btool
# get re-compiled - otherwise you will end up with either a non-bootable kernel
# or one that gives incorrect branch analysis
#
btoolstart!
	rm -f $(BOOTAREA)/btool_map.count
	rm -f $(BOOTAREA)/*.map
	rm -f $(BOOTAREA)/btool_map

btoolsubdirs!
	@RULE=branch; export $$RULE; $(KSUBDIRS_MAKERULE)

unix.kbtool!$(PROD)version installkernel
	cd btool; $(MAKE)
	$(LBOOT) $(LBOOTOPTS) -m $(ROOT)/$(MASTERDIR) -b $(ROOT)/$(CPUINSTDIR) \
		-u $(ROOT)/$(CPUINSTDIR)/$@ -s $(ROOT)/$(SYSTEM_KDEBUG) \
		-c $(STUNEFILE) -n $(ROOT)/$(MTUNEDIR)
	$(SETSYM) $(ROOT)$(CPUINSTDIR)/$@
	cd $(BOOTAREA) ;\
	rm -f btool_map ;\
	cat *.map >btool_map

#
# fastclobber - clobber all products quickly - doesn't touch $ROOT/ysr/sysgen
# so as not to disturb other subsystems.
fastclobber!
	rm -fr *bootarea
	find . -name Makedepend\* | xargs rm -f
