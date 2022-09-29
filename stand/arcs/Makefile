#
# Makefile to build the complete standalone system for a particular product,
# which currently means a particular cpu board.
# 
# $Revision: 1.125 $
# $Date: 1999/11/01 21:35:24 $
#
DEPTH=.
CTAGFILES = -name '*.[chsy]'
include commondefs
include $(PRODUCTDEFS)
DEF_CSTYLE=

# Allow additional directories to be built based on current product.
#
4D40_X=\#
4D40_O32_X=\#
4DACE1_X=\#
4DACE1_O32_X=\#
4DACE1_64_X=\#
4DMH_X=\#
4DMH_O32_X=\#
EVEREST_X=\#
EVEREST32_X=\#
BB_EVEREST_X=\#
TETON_X=\#
T5_EVEREST_X=\#
RACER_X=\#
RACER_RPROM_X=\#
SN0_X=\#
SN0XXL_X=\#
SN0PROM_X=\#
SN0XXLPROM_X=\#
SN01_X=\#
SN0S_X=\#
T5I2_X=\#
$(PRODUCT)_X=

DEFDIRS_NOSYMMON= include lib sash ide
DEFDIRS= $(DEFDIRS_NOSYMMON) symmon

ELFTOCOFF= tools/elf2coff

$(EVEREST32_X)DIRS= tools lib sash ide IO4prom
$(EVEREST_X)DIRS= lib symmon
$(BB_EVEREST_X)DIRS= tools $(DEFDIRS) IO4prom
$(T5_EVEREST_X)DIRS= tools include lib sash symmon ide IO4prom
$(4D40_X)DIRS= $(ELFTOCOFF) lib symmon
$(4D40_O32_X)DIRS= $(ELFTOCOFF) $(DEFDIRS_NOSYMMON)
$(TETON_X)DIRS= $(DEFDIRS)
$(T5I2_X)DIRS= $(DEFDIRS)
$(4DACE1_X)DIRS= $(ELFTOCOFF) lib symmon
$(4DACE1_O32_X)DIRS= $(ELFTOCOFF) $(DEFDIRS_NOSYMMON) 
$(4DACE1_64_X)DIRS= $(ELFTOCOFF) include lib symmon
$(RACER_X)DIRS= tools/convert $(DEFDIRS) IP30prom/Released
$(RACER_RPROM_X)DIRS= tools/convert include lib ide
$(SN0_X)DIRS= lib lib/libkl tools IO6prom IP27prom symmon
$(SN0XXL_X)DIRS= lib lib/libkl tools IO6prom IP27prom symmon
$(SN0PROM_X)DIRS= lib lib/libkl tools IO6prom
$(SN0XXLPROM_X)DIRS= lib lib/libkl tools IO6prom
$(SN01_X)DIRS= lib lib/libkl symmon 
$(SN0S_X)DIRS= lib lib/libkl tools symmon IO6prom
$(4DMH_X)DIRS= $(ELFTOCOFF) lib symmon IP32prom
$(4DMH_O32_X)DIRS= $(DEFDIRS_NOSYMMON) 

# This is used in the recursive make rules.
PRODDIRS = IP19* IP20* IP21* IP22* IP25* IP26* IP27* IP30* IP32*
EXPORTS_SUBDIRS=lib include

COMMONPREF=stand

# this allows a developer to specify a different default,
# which is useful if you are working on proms, for example
sinclude localdefault

# targets hit by build group--use only EVERYPRODUCT
default install .DEFAULT:
	@if [ -z "$$PRODUCT" ] ; then for prod in $(EVERYPRODUCT); \
	  do PRODUCT=$$prod; export PRODUCT; $(MAKE) $@; done; \
	  else DIRS=" ${DIRS}"; \
		for d in $$DIRS; do \
			echo "====\tcd $$d; $(MAKE) $@"; \
			( cd $$d; ${MAKE} $@ );  \
		done; fi
	$(INSTALL) -idb STANDFRIEND -F /usr/include/make -m 444 -src commondefs commondefs.standarcs
	$(INSTALL) -idb STANDFRIEND -F /usr/include/make -m 444 -src commonrules commonrules.standarcs
	$(INSTALL) -idb STANDFRIEND -F /usr/include/make -m 444 ENDIANdefs MIdefs R4000defs R400064defs R4000N32defs TFP64defs R1000064defs MIPS64defs MIPSN32defs

# other things that can get EXTRAPRODUCT w/o harm
clean rmtargets clobber tags fluff:
	@if [ -z "$$PRODUCT" ] ; then for prod in $(EVERYPRODUCT) $(EXTRAPRODUCT); \
	  do PRODUCT=$$prod; export PRODUCT; $(MAKE) $@; done; \
	  else DIRS="${DIRS} ${OPTDIRS}"; \
		for d in $$DIRS; do \
			echo "====\tcd $$d"; \
			( cd $$d; ${MAKE} $@ );  \
		done; fi

include $(COMMONRULES)

$(COMMONTARGS): $(COMMONPREF)$$@

$(COMMONPREF)clobber: lclobber

# the second line is so that all the product subdirs get clobbered,
# not just the one that $PRODUCT uses
# the 'machine independent' (actually multiple product) dirs are
# handled directly from the clobber rule.
# note that for some of the dirs, PRODUCT gets set 'wrong', but this will
# have to be handled individually in those dirs.
lclobber:
	for d in $(EVERYPRODUCT) ; do rm -rf $$d.O $${d}p.O $${d}cp.O; done
	@set +e; for d in $(PRODDIRS); do \
	(cd $$d && echo ==== $$d clobber && \
	case $$d { \
	IP19*) PRODUCT=EVEREST;;\
	IP20*) PRODUCT=4D40;;\
	IP21*) PRODUCT=BB_EVEREST;;\
	IP22*) PRODUCT=4DACE1;;\
	IP26*) PRODUCT=TETON;;\
	IP27XXL) PRODUCT=SN0XXL;;\
	IP27*) PRODUCT=SN0;;\
	IP28*) PRODUCT=T5I2;;\
	IP25*) PRODUCT=T5_EVEREST;;\
	IP30*) PRODUCT=RACER;;\
	IP32*) PRODUCT=4DMH;;\
	} &&  export PRODUCT && $(MAKE) clobber); done

# descend into the subdirs where prom libs are made, and make the
# pseudo targets that make the libs; then make the prom.
dprom prom fprom.bin:
	@echo "====\tcd tools/convert"; (cd tools/convert; $(MAKE)) ;
	@echo "====\tcd lib"; (cd lib; $(MAKE)) ;
	@echo "====\tcd ide"; (cd ide; $(MAKE) prom);
	@echo "====\tcd $(CPUBOARD)prom"; (cd $(CPUBOARD)prom; $(MAKE) $@);

# make all IP27prom targets
# The IP27prom contains an embedded copy of the IO6prom - this is because
# the O200's (SN00) have only one physical prom - no separate physical
# IO6prom part.  In order for the IP27prom image to fit on the flash part,
# some graphics libraries must be left out of the IO6prom during
# compilation.  However, bigger SN0's (i.e. Onyx2's) need those graphics
# libraries in the IO6prom they use.
#
# We solve this by having 2 separate PRODUCTs - when the IO6prom is built
# with product SN0, graphics libraries are not included.  When compiled
# with product SN0PROM, graphics libraries _are_ included.  It is critical
# that the IO6prom images which are checked in are built with product
# SN0PROM, _but_ that the IP27prom images are built with product SN0 so
# they get the stripped-down version of the IO6prom.  (Note that this
# means that the IO6prom directory must be built with both products - one
# for the IO6prom images, and one for inclusion in the IP27prom images).
#
# For the 256+p proms, SN0XXL corresponds to SN0 and SN0XXLPROM
# corresponds to SN0PROM.

IP27proms:
	@for prod in SN0 SN0PROM SN0XXL SN0XXLPROM; do \
		echo "====\tcd lib for product $$prod"; \
		(cd lib; $(MAKE) PRODUCT=$$prod) ; \
		echo "====\tcd IO6prom for product $$prod"; \
		(cd IO6prom; $(MAKE) PRODUCT=$$prod) ; \
	done
	@for prod in SN0 SN0XXL; do \
		echo "====\tcd IP27prom for product $$prod"; \
		(cd IP27prom; $(MAKE) PRODUCT=$$prod) ; \
	done

# make the IP27prom binary images
IP27binaries: IP27proms
	@echo "====\tcd IO6prom; p_modify SN0*.uunc; make uunc"; \
	(cd IO6prom; p_modify SN0*.uunc; $(MAKE) PRODUCT=SN0PROM uunc; $(MAKE) PRODUCT=SN0XXLPROM uunc);
	@echo "====\tcd IP27prom; p_modify SN0*.uunc; make uunc"; \
	(cd IP27prom; p_modify SN0*.uunc; $(MAKE) PRODUCT=SN0 uunc; $(MAKE) PRODUCT=SN0XXL uunc);
	@echo "Binary images created.  To check images into the tree," 
	@echo "first edit the include/release.mk file as described in the" 
	@echo "file.  Then do a p_integrate and p_finalize."

# make all IP30prom targets
IP30proms:
	@echo "====\tcd tools/convert"; (cd tools/convert; $(MAKE)) ;
	@echo "====\tcd lib"; (cd lib; $(MAKE)) ;
	@echo "====\tcd ide"; (cd ide; $(MAKE) prom);
	@echo "====\tcd $(CPUBOARD)prom"; (cd $(CPUBOARD)prom; $(MAKE));
	@echo "====\tcd lib; make PRODUCT=RACER_RPROM"; (cd lib; $(MAKE) PRODUCT=RACER_RPROM);
	@echo "====\tcd ide; make PRODUCT=RACER_RPROM prom"; (cd ide; $(MAKE) PRODUCT=RACER_RPROM prom);
	@echo "====\tcd $(CPUBOARD)prom; make PRODUCT=RACER_RPROM rprom"; (cd $(CPUBOARD)prom; $(MAKE) PRODUCT=RACER_RPROM rprom);
	@echo "====\tcd $(CPUBOARD)prom; make IP30prom.bin"; (cd $(CPUBOARD)prom; $(MAKE) IP30prom.bin);

# exports no longer a nop, because we now export the libraries
# needed to build fx, so fx moves completely over to the irix
# ism, to avoid cross ism symlinks.  If PRODUCT is not set, do
# all, else do just the one for PRODUCT.
# can't use $(EXPORTS_SUBDIRS_MAKERULE), because it has a leading @...
# this is somewhat overkill, since all arcs will make is a single
# library for all products at the moment, but it is worthwhile 
# preparing for the future, and all the lib stuff has to built
# at some point anyway...
# note that R[34]000defs in arcs are a superset of those in
# classic, so we install just the arcs versions
exports:
	if [ -z "$(PRODUCT)" ]; then for prod in $(EVERYPRODUCT); do \
	(PRODUCT=$$prod; export PRODUCT; \
		for d in $(EXPORTS_SUBDIRS); do $(SUBDIR_MAKERULE); done ) ; \
	done; else for d in $(EXPORTS_SUBDIRS); do $(SUBDIR_MAKERULE); done ; fi
	$(INSTALL) -F /usr/include/make -m 444 -src commondefs commondefs.standarcs
	$(INSTALL) -F /usr/include/make -m 444 -src commonrules commonrules.standarcs
	$(INSTALL) -F /usr/include/make -m 444 ENDIANdefs MIdefs R4000defs R400064defs R4000N32defs TFP64defs R1000064defs MIPS64defs MIPSN32defs

TAGS: $(COMMONPREF)TAGS
