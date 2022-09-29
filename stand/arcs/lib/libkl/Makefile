# Makefile for libkl.a
#
# libkl contains SN0 specific code that is shared between IP27prom and
# IO6prom (and probably Speedo).

#ident "$Revision: 1.7 $"
KLDEPTH=.
include $(KLDEPTH)/libkldefs

SUBDIRS= ml io hwreg diags fru
LIBNAME=libkl.a

default: ${LIBNAME}

install: default

# We use the COMMONPREF variable to prefix the commonrule target names with
# "saio" so that we can define our own depend, clean, etc. below.
COMMONPREF= libkl
include $(KLDEPTH)/libklrules

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
