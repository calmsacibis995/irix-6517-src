#ident $Revision: 1.4 $
#

IDB_PATH = /usr/share/catman/p_man/cat3s/ftn
IDB_TAG  = irix.man.ftn

include $(ROOT)/usr/include/make/mandefs

#
# Build pages for Fortran
#
LANGOPT = f

#
# Path to the real directory containing the manpage source
#
MDIR = ../../man3s

#
# List of pages in the real source directory that contain .Op f directives
#
FTN_PAGES = setbuf.3s

MANPREF = ftn

clean:		$(MANPREF)clean

default install: make_links
		$(MAKE) $(MANPREF)$@

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
