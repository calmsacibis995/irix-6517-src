# Makefile for libsk.a
#
# libsk is the standalone kernel.  It gets built into the PROM and
# other ``special'' programs like ide.  Machine independent programs do
# not link with it.

#ident "$Revision: 1.7 $"
SKDEPTH=.
include $(SKDEPTH)/libskdefs

SUBDIRS= lib io fs ml net cmd graphics
LIBNAME=libsk.a

default: ${LIBNAME}

install: default

# We use the COMMONPREF variable to prefix the commonrule target names with
# "saio" so that we can define our own depend, clean, etc. below.
COMMONPREF= libsk
include $(SKDEPTH)/libskrules

${LIBNAME}: ${SUBDIRS}

${SUBDIRS}: ${_FORCE}
	@echo "======\tcd $@"; \
	( cd $@; exec ${MAKE} );

# Common rules which we implement at this level (saiodepend, saioclean, etc.)
# and also pass on to sub-makes.
clean:
	for i in $(EVERYPRODUCT) $(EXTRAPRODUCTS); do rm -f $${i}*.O/*.o; done

rmtargets:
	for tdir in $(EVERYPRODUCT) $(EXTRAPRODUCTS);		\
	do							\
		rm -f $${tdir}.O/$(LIBNAME);			\
	done

clobber: lclobber

# need this extra rule to clean up all products; rmtargets and clean
# don't since in the normal course there isn't anything to clean up
# in the subdirs
lclobber:
	for i in $(EVERYPRODUCT) $(EXTRAPRODUCTS); do rm -rf $${i}*.O; done

depend incdepend lint: $(_FORCE)
	@for i in $(SUBDIRS); \
	do \
		echo "======\tcd $$i; $(MAKE) $@"; \
		( cd $$i; $(MAKE) $@ ); \
	done

clobber fluff tags: ${COMMONPREF}$$@
	@for i in ${SUBDIRS}; do \
		echo "======\tcd $$i; ${MAKE} $@"; \
		( cd $$i; ${MAKE} $@; )\
	done
