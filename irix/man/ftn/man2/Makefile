#ident $Revision: 1.7 $
#

IDB_PATH = /usr/share/catman/p_man/cat2/ftn
IDB_TAG  = irix.man.ftn


include $(ROOT)/usr/include/make/mandefs

#
# Build pages for Fortran
#
LANGOPT = f

#
# Relative path to directory containing the c pages
#
MDIR = ../../man2

#
# List of pages in the other directory that contain .Op f directives
#
FTN_PAGES =	sgikopt.2 warnings

MANPREF = ftn

default install: make_links
		$(MAKE) $(MANPREF)$@

clean:		$(MANPREF)clean

clobber:	$(MANPREF)clobber rm_links

include $(ROOT)/usr/include/make/manrules

make_links:
	@echo "Making links to source pages ..."
	-@for file in $(FTN_PAGES) ; do \
		rm -f $$file ; \
		ln -s $(MDIR)/$$file . ; \
	done

rm_links:
	-@find . -type l -print | xargs rm -f "{}" ";"
