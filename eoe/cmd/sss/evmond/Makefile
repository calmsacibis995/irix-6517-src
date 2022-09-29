#!smake
#
# Makefile for my_ism
#
#
#ident "$Revision: 1.1 $"

### 1) ----- include ismcommondefs *before* your own definitions -----
### ismcommondefs sets some definitions and then includes commondefs
include $(ROOT)/usr/include/make/commondefs
include ../include/ssscommondefs

### 2) -------------- give definitions of your own --------------
### these are directories in which to run rawidb phase
SUBDIRS=misc pthreads seh dsm 
LCINCS=-Iinclude -Ipthreads
LCDEFS=-D_POSIX_C_SOURCE=199506L -g
COMMONPREF=sss_
SWDEST=/usr/etc
SWSUBSYS=$(SSS_EOE)

OBJECT_STYLE=N32
LD_QUICKSTART_INFO=
LWOFF=
TARGETS=eventmond
SCRIPT=eventmonitor
LCINCS=-nostdinc -I$(ROOT)/usr/include -I$(ROOT)/usr/include/sss \
	-Iinclude -Ipthreads


default install:$(COMMONPREF)$$@ $(_FORCE) semd.o eventmond.o
	$(SUBDIRS_MAKERULE)
	$(CCF)  -o $(TARGETS) misc/alloc.o \
		pthreads/sss_pthreads.o misc/Dlist.o semd.o eventmond.o misc/strings.o \
		seh/libseh.a dsm/libdsm.a \
		$(LDOPTS) $(LDLIBS) -Lmisc \
		-lpthread -lalloc -lm -llmsgi
	$(INSTALL) -idb $(SWSUBSYS) -F $(SWDEST) $(TARGETS)
	$(INSTALL) -idb $(SWSUBSYS) -F /etc/init.d $(SCRIPT)

$(COMMONTARGS):$(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)

include $(COMMONRULES)

### We are not overriding commonrules. for default and install.
$(COMMONPREF)default $(COMMONPREF)clean $(COMMONPREF)clobber $(COMMONPREF)rmtargets:
$(COMMONPREF)install:

$(SUBDIRS):$(_FORCE)
	cd $@; $(MAKE)

### rule for rawidb phase (make sure to set envariable RAWIDB)
rawidb:$(_FORCE)
	@RAWIDB=$(IDBFILE); export RAWIDB ; \
	if [ -f $$RAWIDB ] ; then mv $$RAWIDB $$RAWIDB.prev ; fi ;\
	echo "RAWIDB=$$RAWIDB SRC=$(SRC) $(MAKE) install" ;\
	$(MAKE) SRC=$(SRC) install


