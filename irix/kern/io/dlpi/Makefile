#!smake
# $Revision: 1.4 $

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/dlpi
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

#ifndef SRC_PRD
SUBDIRS=snif llc2 xtimer
#else
SUBDIRS=snif
#endif
HEADERS_SUBDIRS=sys

TARGETS=

#
# Rules
#
# If PRODUCT is set then the $(PROD) rules are used else
# the $(NOPROD) rules are used.
# We set KOPREF to override the rules in kcommonrules
#
KOPREF=kern
include ${DEPTH}/kcommonrules

#
# Rules when PRODUCT is set
#
$(PROD)install!$(BOOTAREA) bootheaders headers installsubdirs $(TARGETS)

$(PROD)default!$(BOOTAREA) $(SUBDIRS) $(TARGETS)

$(PROD)subdirs!.RECURSIVE
$(PROD)subdirs!
	@RULE=default; export $$RULE; $(KSUBDIRS_MAKERULE)

$(PROD)headers!doheadersubdirs

doheadersubdirs!.RECURSIVE
doheadersubdirs!
	@RULE=headers; export $$RULE; \
	for d in $(HEADERS_SUBDIRS); do $(SUBDIR_MAKERULE); done

$(PROD)bootheaders!
	@RAWIDB= ; export RAWIDB; \
	RULE=headers; export $$RULE; \
	for d in $(HEADERS_SUBDIRS); do $(SUBDIR_MAKERULE); done

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

dormtargetssubdirs!.RECURSIVE
dormtargetssubdirs!
	@RULE=rmtargets; export $$RULE; $(KSUBDIRS_MAKERULE)

#
# Rules when PRODUCT is not set
#
$(NOPROD)install $(NOPROD)default $(NOPROD)version $(NOPROD)subdirs \
$(NOPROD)clean $(NOPROD)rmtargets!
	@for p in $(EVERYPRODUCT) ; do \
		PRODUCT=$$p; export PRODUCT ;\
		echo "PRODUCT=$$p $(MAKE) $(@:S/${NOPROD}/$(PROD)/)" ;\
		$(MAKE) $(@:S/${NOPROD}/$(PROD)/) ;\
	done

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
