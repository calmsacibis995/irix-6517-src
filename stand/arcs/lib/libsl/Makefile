# Makefile for libsl.a
#
# libsl is the standalone loader.  It gets built into the PROM and
# other ``special'' programs like sash.  Machine independent programs do
# not link with it.

#ident "$Revision: 1.1 $"
SLDEPTH=.
include $(SLDEPTH)/libsldefs

SUBDIRS= lib
LIBNAME=libsl.a

default: ${LIBNAME}

install: default

# We use the COMMONPREF variable to prefix the commonrule target names with
# "saio" so that we can define our own depend, clean, etc. below.
COMMONPREF= libsl
include $(SLDEPTH)/libslrules

${LIBNAME}: ${SUBDIRS}

${SUBDIRS}: ${_FORCE}
	@echo "======\tcd $@"; \
	( cd $@; exec ${MAKE} );

# Common rules which we implement at this level (saiodepend, saioclean, etc.)
# and also pass on to sub-makes.

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
	for i in ${SUBDIRS}; do \
		echo "======\tcd $$i; ${MAKE} $@"; \
		( cd $$i; ${MAKE} $@; )\
	done
	for i in $(EVERYENDIAN); do rm -rf $${i}*.O ; done
	
tags: ${COMMONPREF}$$@
	@for i in ${SUBDIRS}; do \
		echo "======\tcd $$i; ${MAKE} $@"; \
		( cd $$i; ${MAKE} $@; )\
	done

