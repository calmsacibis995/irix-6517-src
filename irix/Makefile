#! smake
#
# Makefile for irix ism
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
# rawidb:executes 'install' with RAWIDB set to the file that the ism image
#	generation makefile (in build/) expects. As with 'install'
#	'headers' and 'exports' must be run beforehand.
#
# ism:descends into the build directory and performs the steps to generate
#	the finalidb. At this point images may be generated.
#	'rawidb' must have already been run.
#
# images: descends into the build directory and builds ism images.
#	'rawidb' and 'ism' must have been run previously.
#
# clean:removes .o's
# clobber:removes all non-source. The effective of 'clobber' can be checked
#	by running p_check -w after running 'clober'
# rmtargets:removes TARGETS only
#
include $(ROOT)/usr/include/make/ismcommondefs

SUBDIRS=include lib cmd kern man build 
COMMONPREF=irix
HEADERS_SUBDIRS=include kern lib cmd
EXPORTS_SUBDIRS=lib cmd
SRC!=pwd
IDBFILE=${SRC}/build/IDB
ISM_NAME=irix

#ifndef SRC_PRD
default:headers exports $(_FORCE)
#else
default: $(_FORCE)
#endif
	$(SUBDIRS_MAKERULE)

#$(COMMONTARGS) install:$(COMMONPREF)$$@ $(_FORCE)
$(COMMONTARGS) install: $(_FORCE)
	$(SUBDIRS_MAKERULE)

include $(ISMCOMMONRULES)

headers:$(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports:$(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

#
# This top level doesn't need to do anything special for these targets
#
$(COMMONPREF)default $(COMMONPREF)clean $(COMMONPREF)clobber $(COMMONPREF)rmtargets:

$(SUBDIRS):$(_FORCE)
	cd $@; $(MAKE)

rawidb:$(_FORCE)
	@RAWIDB=$(IDBFILE); export RAWIDB ; \
	if [ -f $$RAWIDB ] ; then mv $$RAWIDB $$RAWIDB.prev ; fi ;\
	echo "RAWIDB=$$RAWIDB SRC=$(SRC) $(MAKE) install" ;\
	$(MAKE) SRC=$(SRC) install

ism:$(_FORCE)
	cd build; $(MAKE) buildism

images:$(_FORCE)
	cd build; $(MAKE) buildimages
