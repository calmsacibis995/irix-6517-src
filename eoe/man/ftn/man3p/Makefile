#ident $Revision: 1.5 $
#

IDB_PATH = /usr/share/catman/p_man/cat3p/ftn
IDB_TAG  = irix.man.ftn

include $(ROOT)/usr/include/make/mandefs

#
# Build pages for Fortran
#
LANGOPT = f

#
# Path to the real directory containing the manpage source
#
MDIR = ../../man3p

#
# List of pages in the real source directory that do not
# contain .Op f directives
#
NO_FTN_PAGE =	amalloc.3p test_and_set.3p pthread*

MANPREF = ftn

default install: make_links
		$(MAKE) $(MANPREF)$@

clean:		$(MANPREF)clean

clobber:	$(MANPREF)clobber rm_links

include $(ROOT)/usr/include/make/manrules

make_links:
	@echo "Making links to source pages ..."
	-@rm -f *.3p
	-@ln -s $(MDIR)/*.3p .
	-@rm -f $(NO_FTN_PAGE)

rm_links:
	-@find . -type l -print | xargs rm -f "{}" ";"
