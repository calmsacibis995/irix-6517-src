# Makefile for libsc.a
#
# libsc.a is the machine independent standalone C runtime support library.
#
# $Revision: 1.4 $

SCDEPTH=.
include $(SCDEPTH)/libwarscdefs

SUBDIRS= cfg cmd fs lib ml gui xfs
LIBNAME=libsc.a
# don't want the '/./' in the name, so don't use TARGETDIR
INSTDIR=/usr/lib/stand/$(ENDIANND)$(CPUARCH_PFIX).O

default install: ${LIBNAME}

# not extported currently
#	cd $(TARGETDIR); $(INSTALL) -F $(INSTDIR) -m 444 -idb STANDFRIEND" nostrip" $(LIBNAME)

# We use the COMMONPREF variable to prefix the commonrule target names with
# "libsc" so that we can define our own clean, etc. below.
COMMONPREF= libsc
include $(SCDEPTH)/libwarscrules
EVERYENDIAN=IP28BE64

${LIBNAME}: tlink ${SUBDIRS}

tlink:
	@( for i in $(SUBDIRS); do tlink ../libsc/$$i $$i ; done )
	@ln -fs libwarscdefs libscdefs
	@ln -fs libwarscrules libscrules

${SUBDIRS}: ${_FORCE}
	@echo "======\tcd $@"; \
	( cd $@; exec ${MAKE} );

clean:
	for i in $(EVERYENDIAN); do rm -f $${i}*.O/*.ln $${i}*.O/*.o; done

rmtargets:
	for tdir in $(EVERYENDIAN);		\
	do					\
		rm -f $${tdir}.O/$(LIBNAME);	\
	done

depend incdepend: $(_FORCE)
	@for i in $(SUBDIRS); \
	do \
		echo "======\tcd $$i; $(MAKE) $@"; \
		( cd $$i; $(MAKE) $@ ); \
	done

subdirlint: $(_FORCE)
	@for i in $(SUBDIRS); \
	do \
		echo "======\tcd $$i; $(MAKE) lint"; \
		( cd $$i; $(MAKE) lint); \
	done

lint: subdirlint
	cd $(ENDIANND).O; \
	rm -f llib-lsc.ln; $(LINT) $(LINTLLFLAGS) -o sc *.ln

clobber: $(_FORCE) ${COMMONPREF}clobber
	rm -rf $(SUBDIRS)
	rm -f libscdefs libscrules
	for i in $(EVERYENDIAN); do rm -rf $${i}*.O ; done
	
tags: ${COMMONPREF}$$@
	@for i in ${SUBDIRS}; do \
		echo "======\tcd $$i; ${MAKE} $@"; \
		( cd $$i; ${MAKE} $@; )\
	done

# exports no longer a nop, because we now export the libraries
# needed to build fx, so fx moves completely over to the irix
# ism (to avoid cross ism symlinks)
# Don't use the PRODUCT name here, since all of the same endianness
# are the same (may require more fixup for 64 bit...)
exports: default
	$(INSTALL) -F /usr/lib/stand/$(ENDIANND)$(CPUARCH_PFIX).O -m 444 -idb "nostrip" -src $(TARGETDIR)/$(LIBNAME) $(LIBNAME)
