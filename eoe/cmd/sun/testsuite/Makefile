#
#       @(#)Makefile	1.2 97/01/03 Connectathon Testsuite
#
# 'make all'			makes test programs
# 'make clean'			cleans directories
# 'make copy DESTDIR=path'	copies test programs to path
# 'make dist DESTDIR=path'	copies sources to path
include $(ROOT)/usr/include/make/commondefs

CVERSION=-cckr
CFILES= domount.c getopt.c

DESTDIR=/no/such/path
COPYFILES= $(CFILES) Testitems README runtests server tests.init
MAKEDIRS= basic general special lock
SUBDIRS= $(MAKEDIRS) tools
TARGETS= $(CFILES:.c=)

default: $(TARGETS) $(MAKEDIRS)

COMMONPREF= test
include $(COMMONRULES)

$(MAKEDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(COMMONTARGS): $(COMMONPREF)$$@
	@for d in $(SUBDIRS); do \
		$(ECHO) "====\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

install: $(_FORCE)
	@$(ECHO) "The NFS testsuite is not installed."

domount: domount.c
	$(CC) $(CFLAGS) -o $@ $@.c
#	chown root domount
#	chmod u+s domount

getopt: getopt.c
	$(CC) $(CFLAGS) -o $@ $@.c

copy: mknewdirs
	cp $(COPYFILES) $(DESTDIR)
	cd basic; $(MAKE) copy DESTDIR=$(DESTDIR)/basic
	cd general; $(MAKE) copy DESTDIR=$(DESTDIR)/general
	cd special; $(MAKE) copy DESTDIR=$(DESTDIR)/special
	cd tools; $(MAKE) copy DESTDIR=$(DESTDIR)/tools
	cd lock; $(MAKE) copy DESTDIR=$(DESTDIR)/lock

dist: mknewdirs
	cp Makefile $(COPYFILES) $(DESTDIR)
	cd basic; $(MAKE) dist DESTDIR=$(DESTDIR)/basic
	cd general; $(MAKE) dist DESTDIR=$(DESTDIR)/general
	cd special; $(MAKE) dist DESTDIR=$(DESTDIR)/special
	cd tools; $(MAKE) dist DESTDIR=$(DESTDIR)/tools
	cd lock; $(MAKE) dist DESTDIR=$(DESTDIR)/lock

mknewdirs:
	-mkdir $(DESTDIR)/basic $(DESTDIR)/general $(DESTDIR)/special \
	       $(DESTDIR)/tools $(DESTDIR)/lock
