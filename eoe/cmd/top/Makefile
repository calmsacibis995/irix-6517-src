#!smake
#
# Makefile for top 3.4
# This source code is freeware.
# Copyright belongs to William LeFebvre
#
# "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

SRCDIR = top-3.4
# TARGETS = top top.z
TARGETS = top
# SRC != pwd
# SETGID = -u root -g sys -m 2755
NORMAL = -u root -g sys -m 755
RO = -u root -g sys -m 444
CONF_FILES = $(SRCDIR)/machine/m_irix.c $(SRCDIR)/machine/m_irix.man
DEFAULTS = defaults.irix
MANPAGE = ../../man/man1/top.1


default rawidb: $(TARGETS)

install: default
	$(INSTALL) -F /usr/sbin $(NORMAL) top
	$(INSTALL) -F /usr/sbin -idb "nostrip" $(NORMAL) gr_top
# -- the top.1 man page was moved to irix/man/man1 - see below
#	$(INSTALL) -F /usr/share/catman/u_man/cat1 $(RO) top.z

# include $(COMMONRULES)
top: config.done
	cd $(SRCDIR); $(MAKE) -f Makefile.sgi
	cp $(SRCDIR)/top .


# Don't run configure irix, it may break builds
# chmod +w Makefile top.local.h; ./Configure irix;
#
config config.done:
	cd $(SRCDIR); \
	rm -f Makefile; cp -f Makefile.sgi Makefile; \
	cp -f ../defaults.irix .defaults; \
	ln -fs machine/m_irix.c machine.c; \
	touch ../config.done

clobber clean: src_clobber
	-rm -f $(TARGETS)

# Chicken & egg problem:
#	Top level build first does a 'make clobber' or 'make clean'
#	But we don't have a 'Makefile' before we do a ./Configure
#	later. So I just keep a temporary configured 'Makefile.sgi'
#	for this. Sigh.
src_clobber src_clean:
	cd $(SRCDIR); $(MAKE) -f Makefile.sgi clean

# - Create the compressed man page (outside the source tree)
# Not done anymore. man page moved to irix/man/man1
# YOU NEED TO MANUALLY BUILD IT (using "make exportman")
# WHENEVER THE MAN PAGE CHANGES
manpage man: $(MANPAGE)

$(MANPAGE): $(SRCDIR)/top.1
	# keep a preformated copy here, for reference
	neqn $(SRCDIR)/top.1 | tbl | nroff -man | col > top.man
	# pack top.man && mv top.man.z top.z

exportman: $(MANPAGE)
	p_modify $(MANPAGE) && p_integrate $(MANPAGE)
	cp $(SRCDIR)/top.1 $(MANPAGE)
	p_finalize -B -S $(MANPAGE)
