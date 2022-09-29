#!smake
#
# Makefile for ismname perl5 (part of eoe?)
#
############################################################################
# The targets you must handle, i.e. the interface between the outside SGI
#	product and the SGI Makefile conventions:
#
# src_default: make it build all of the outside SGI product source.
#
# src_clean: make it get rid of .o's in the outside SGI product.
#
# src_clobber: make it remove all non source in the outside SGI product.
#
# NOTE: You must also supply contents of the checked in idb file, build/idb
#	see the comments section of it for more detail
#
# NOTE: You will want to link raw man pages from the src subdirectory into
#	the appropriate man subdirectory subdirectory. This will allow
#	the SGI man Makefile conventions to cook the man pages. Or you 
# 	can do it from with the src directory structure, as desired.
#
############################################################################
# The targets handled for you by this Makefile and common SGI include files:
#
# default:this builds all of the software in place. No rawidb is affected and
#	targets are not installed anywhere. No ism image is generated.
#	Invokes src_default target.
#
# startversion: this generates a version number.
#	The developer just needs to invoke the target 
#	(see $(ROOT)/usr/include/make/startversion for more info).
#	The generated version number can be embedded as ident string in 
#	executables by setting the envariable RLS_ID_OBJECT to 
#	${WORKAREA}/.identfile. The VERSION envariable is also set for use 
#	with the spec file (at gendist time).
#	This target is supplied by ismcommonrules and it saves/documents the 
#	version number in $(WORKAREA)/.version_number. 
#	This target should be handled by the wrapper around your outside
#	of SGI product. ismcommondefs/rules via the template will do this 
#	for you.
#
# buildimages:descends into the build directory and generates the files needed 
#	for the build of entire system. 
#	'rawidb' must be run previous to 'images'.
#
# clean:removes .o's. Invokes src_clean target.
#
# clobber:removes all non-source. The effective of 'clobber' can be checked
#	by running p_check -w after running 'clobber'. Invokes src_clobber.
#
# "$Revision: 1.12 $"

include $(ROOT)/usr/include/make/commondefs

SRCDIR=perl5.004_04
EXT= ext
SUBDIRS=$(SRCDIR) $(EXT) catman build
COMMONPREF=yes
PWD != /bin/pwd
PERL_SRC = ${PWD}/${SRCDIR}
TARGETS=
LDIRT= *~ core

GVERBOSE=-verbose

# clobber clean startversion buildimages default

.NOTPARALLEL: $(SUBDIRS)

default all!	src_default

src_default clobber clean!
	(if test -d $(SRCDIR) ; then cd $(SRCDIR); \
		${TOOLROOTSAFE} ${MAKE} -f Makefile.SGI $@;\
	fi);
	(if test -d $(EXT) ; then cd $(EXT) ;\
		if test -f Makefile ; then \
			PERL_SRC=${PERL_SRC} ${TOOLROOTSAFE} ${MAKE} -f Makefile $@ ;\
		else true; fi \
	else true ; fi);
	(if test -d catman ; then \
		cd catman; ${MAKE} $@; \
	fi);
	(cd build && ${MAKE} $@);

install:	src_default
	(if test -d $(SRCDIR) ; then cd $(SRCDIR); \
		INSTALLDIRS="perl" ${MAKE} -f Makefile.SGI $@; \
	fi);
	(if test -d $(EXT) ; then cd $(EXT) ;\
		if test -f Makefile ; then \
			PERL_SRC=${PERL_SRC} ${TOOLROOTSAFE} ${MAKE} -f Makefile $@ ;\
		else true; fi \
	else true ; fi);
#	(test -d build && cd build && ${MAKE} $@);

rawidb:	src_default
	(if test -d ${SRCDIR} ; then cd ${SRCDIR}; \
		SRC=${SRC} RAWIDB=${RAWIDB} ${MAKE} -f Makefile.SGI install; \
	fi );
	(if test -d $(EXT) ; then cd $(EXT) ;\
		if test -f Makefile ; then \
			SRC=${SRC} RAWIDB=${RAWIDB} PERL_SRC=${PERL_SRC} ${TOOLROOTSAFE} ${MAKE} -f Makefile install;\
		else true; fi \
	else true ; fi );
#	(test -d build && SRC=${SRC} RAWIDB=${RAWIDB} cd build && ${MAKE} $@);

images:
#	(test -d build && cd build && ${MAKE} $@);

include $(ROOT)/usr/include/make/startversion
