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

# since we define commondefs, etc they are referenced from their source
# we also cannot include ismcommondefs here since it includes commondefs
# So, don't use this makefile as a template!
#
RULESDIR=include/makerules
include $(RULESDIR)/commondefs
RELEASEDEFS=$(RULESDIR)/releasedefs
COMMONRULES=$(RULESDIR)/commonrules

SUBDIRS= include lib cmd man build relnotes books
COMMONPREF=eoe
HEADERS_SUBDIRS=include lib cmd
EXPORTS_SUBDIRS=lib cmd
SRC!=pwd
IDBFILE=${SRC}/build/IDB
ISM_NAME=eoe

#ifndef SRC_PRD
default:headers exports $(_FORCE)
#else
default: $(_FORCE)
#endif
	$(SUBDIRS_MAKERULE)

$(COMMONTARGS) install:$(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)

# Since this ism defines ismcommonrules we must assume it isn't installed yet!
include $(RULESDIR)/ismcommonrules

#
# INSTALL rules for transitional IRIX ism installation
# This rule needs to do any replaces rules and removal
# of binaries that have changed location
# Also handles any temprorary linking to maintain compatibility
#
$(COMMONPREF)install:
	SRC=$(SRC) $(INSTALL) -F /etc -lns ../usr/sbin/prfld prfld
	SRC=$(SRC) $(INSTALL) -F /etc -lns ../usr/sbin/prfdc prfdc
	SRC=$(SRC) $(INSTALL) -F /etc -lns ../usr/sbin/prfpr prfpr
	SRC=$(SRC) $(INSTALL) -F /etc -lns ../usr/sbin/prfsnap prfsnap
	SRC=$(SRC) $(INSTALL) -F /etc -lns ../usr/sbin/prfstat prfstat
	SRC=$(SRC) $(INSTALL) -F /usr/bin -lns ../lib/iaf/scheme login
	SRC=$(SRC) $(INSTALL) -F /usr/bsd -lns ../bin/more more
	SRC=$(SRC) $(INSTALL) -F /usr/bsd -lns ../lib/more.help more.help
	SRC=$(SRC) $(INSTALL) -F /usr/bsd -lns ../bin/script script
	SRC=$(SRC) $(INSTALL) -F /usr/lib -lns ../share/lib/terminfo terminfo
	SRC=$(SRC) $(INSTALL) -idb 'exitop("rm -fr $$rbase/etc/xutmp $$rbase/etc/xwtmp $$rbase/var/sysgen/master.d/master.c $$rbase/bin/rsh")' -dir /

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
