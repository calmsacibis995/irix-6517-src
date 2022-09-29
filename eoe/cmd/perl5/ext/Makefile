#!smake
#
# Makefile for ismname part perl5/ext
#
# Perl5 extensions shipped by SGI
#
# "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

#PERL_SRC assumed defined from parent Makefile
COMMONPREF=yes
TARGETS= MDBM_File
LDIRT= *~ core

GVERBOSE=-verbose

# clobber clean startversion buildimages default

default all!	src_default

src_default clobber clean!
	(for EXT in $(TARGETS) ; do \
	    if test -d $${EXT} ; then ( cd $${EXT} ;\
		if test -f Makefile.SGI ; then \
			INSTALLDIRS="vendor" ${MAKE} -f Makefile.SGI $@ ;\
		else true; fi ) \
	    else true ; fi ;\
	done);


install:	src_default
	(for EXT in $(TARGETS) ; do \
	    if test -d $${EXT} ; then ( cd $${EXT} ;\
		if test -f Makefile.SGI ; then \
			INSTALLDIRS="vendor" ${MAKE} -f Makefile.SGI $@ ;\
		else true; fi ) \
	    else true ; fi ; \
	done );

rawidb:	src_default
	(for EXT in $(TARGETS) ; do \
	    if test -d $${EXT} ; then ( cd $${EXT} ;\
		if test -f Makefile.SGI ; then \
			SRC=${SRC} RAWIDB=${RAWIDB} INSTALLDIRS="vendor" ${MAKE} -f Makefile.SGI $@;\
		else true; fi ) \
	    else true ; fi ; \
	done);


images:
#	(test -d build && cd build && ${MAKE} $@);

include $(ROOT)/usr/include/make/startversion
