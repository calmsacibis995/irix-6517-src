#!smake

# Makefile for standard SMT applications
#
#	$Revision: 1.17 $

DEPTH=..
include $(DEPTH)/Makedefs
SUBDIRS=xpi
.NOTPARALLEL:

LMKDEPFLAGS=$(NULLSUFFIX_MKDEPFLAG)

LLDLIBS	=$(LIBSMT) -lcurses -lm
# do not worry about not using all of the libraries everywhere
LLDOPTS+=-Wl,-woff,84

CFILES	=smtconfig.c smtstat.c smtping.c smtmaint.c smtring.c smtinfo.c

CLIENT	=smtconfig smtstat smtmaint smtring smtinfo
SUCLIENT=smtping
TARGETS	=$(CLIENT) $(SUCLIENT)

INST	=$(INSTALL) -idb fddi.sw.fddi
SUID	=-m 04555 -u root -g sys


#build the tools first
default:subdirs $(TARGETS)

COMMONPREF=smtapps
include $(COMMONRULES)

# Nothing in the subdirectory is installed.  If that ever changes,
# move the "install" target up to here.
$(COMMONTARGS):$(COMMONPREF)$$@ $(_FORCE)
	@for d in $(SUBDIRS); do \
		$(ECHO) "\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

smtconfig:xpi/lc.firm xpi/lc_flash.firm
smtconfig:xpi/mez_d.firm xpi/mez_flash.firm xpi/mez_s.firm
subdirs:$(_FORCE)
xpi/lc.firm xpi/lc_flash.firm xpi/mez_d.firm xpi/mez_flash.firm \
    xpi/mez_s.firm subdirs $(SUBDIRS):
	@for d in $(SUBDIRS); do \
		$(ECHO) "\tcd $$d; $(MAKE)"; \
		cd $$d; $(MAKE); cd ..; \
	done

install $(COMMONPREF)install:default
	$(INST) -F /usr/etc $(CLIENT)
	$(INST) -F /usr/etc $(SUID) $(SUCLIENT)

$(TARGETS):$$@.c $(LIBSMT)


afluff:
	for d in $(TARGETS); do \
		lint $(CFLAGS) $$d.c $(LIBSMT) -lcurses -lm -lc ; \
	done
