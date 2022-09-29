#! smake
#
# Makefile for stand ism
#
# $Revision: 1.4 $
#

include $(ROOT)/usr/include/make/ismcommondefs

SUBDIRS=arcs x86
ALPHA=`date +%y%j%H | sed -e 's/.//'`
ISM_NAME=stand
SRC!=pwd
IDBFILE=${SRC}/build/IDB
COMMONPREF= stand
EXPORTS_SUBDIRS=arcs

# The rest of stand does not understand smake yet, someday...
MAKE=make

default: $(_FORCE)
	$(SUBDIRS_MAKERULE)

include $(ISMCOMMONRULES)

install: $(_FORCE)
	$(SUBDIRS_MAKERULE)

rawidb:$(_FORCE)
	@RAWIDB=$(IDBFILE); export RAWIDB ; \
	if [ -f $$RAWIDB ] ; then mv $$RAWIDB $$RAWIDB.prev ; fi ;\
	echo "RAWIDB=$$RAWIDB SRC=$(SRC) $(MAKE) install" ;\
	$(MAKE) SRC=$(SRC) install

ism:$(_FORCE)
	cd build; $(MAKE) buildism

images:$(_FORCE)
	cd build; $(MAKE) buildimages

clobber:
	$(SUBDIRS_MAKERULE)

#keep higher level makefiles happy
headers:

# exports no longer a nop, because we now export the libraries
# needed to build fx, so fx moves completely over to the irix
# ism (to avoid cross ism symlinks)
exports:$(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)
