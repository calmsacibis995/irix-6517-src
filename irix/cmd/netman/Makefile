#!smake
#
# Makefile for netvis ism
# "$Revision: 1.48 $"
#
# The main targets at the top level are:
#
# default:this performs a bootstrap (headers and exports) then
#	builds all of the software in place. No rawidb is generated and
#	targets are not installed anywhere. No ism image is generated
#
# headers:install all headers. This should always be run without RAWIDB set.
#
# exports:builds and installs all libraries and data files that are required
#	to build the rest of the ism. This should always be run without
#	RAWIDB set.
#
# install:builds and installs the entire ism. 'headers' and 'exports'
#	must be run previous to 'install'. With RAWIDB set this will
#	generate the rawidb for the ism. Without RAWIDB set, all the
#	ism objects will be installed in $ROOT
#
# ism:descends into the build directory and performs the steps to generate
#	the finalidb. At this point images may be generated.
#	'rawidb' must have already been run.
#
# images: descends into the build directory and builds ism images.
#	'rawidb' and 'ism' must have been run previously.
#
# clean:removes .o's
#
# clobber:removes all non-source. The effective of 'clobber' can be checked
#	by running p_check -w after running 'clober'
#
# rmtargets:removes TARGETS only
#
include $(ROOT)/usr/include/make/ismcommondefs

# for now, build only what ships with eoe.  This used to be apps/netman
# but that was too painful for each release, so I've moved it into irix.
# Dave Olson, 5/96
#SUBDIRS= include pidl lib event ui netgraph netlook nettop netvis \
#	 collector netfilters nvlicense rpc.snoopd snmp ftrs ipfilterd \
#	 gifts man books
SUBDIRS= include pidl lib ipfilterd # snmp
HEADERS_SUBDIRS= include ui/include # snmp event ui
EXPORTS_SUBDIRS= pidl lib # snmp event ui
COMMONPREF= netman
SRC= `pwd`
IDBFILE= "`pwd`/build/IDB"
ISM_NAME= netman

default: headers exports $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(COMMONTARGS) install: $(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)

include $(ISMCOMMONRULES)

$(COMMONPREF)install:

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

#
# This top level doesn't need to do anything special for these targets
#
$(COMMONPREF)default $(COMMONPREF)clean $(COMMONPREF)clobber $(COMMONPREF)rmtargets:

$(SUBDIRS):$(_FORCE)
	cd $@; $(MAKE)

rawidb: install

ism:$(_FORCE)
	cd build; $(MAKE) buildism

images:$(_FORCE)
	cd build; $(MAKE) buildimages

.PRECIOUS: ${SUBDIRS} ${HEADERS_SUBDIRS} ${EXPORTS_SUBDIRS}
