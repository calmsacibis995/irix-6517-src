#!smake
#
# Makefile for my_ism
#
#
#ident "$Revision: 1.4 $"

### 1) ----- include ismcommondefs *before* your own definitions -----
### ismcommondefs sets some definitions and then includes commondefs
include $(ROOT)/usr/include/make/commondefs

### 2) -------------- give definitions of your own --------------
### these are directories in which to run rawidb phase
SUBDIRS=seh_interfaces seh_events seh_misc
O_INTERFACES=seh_interfaces/seh_api.o      seh_interfaces/seh_dsm.o     \
	     seh_interfaces/seh_ssdb.o     seh_interfaces/seh_sysid.o   \
	     seh_interfaces/seh_archive_config.o  seh_interfaces/seh_license.o \
	     seh_interfaces/semapisrv.o
O_EVENTS=seh_events/seh_init.o             seh_events/seh_events.o      \
	 seh_events/seh_main.o             seh_events/seh_alarm.o       \
	 seh_events/seh_archive.o
O_MISC=seh_misc/seh_config.o seh_misc/seh_errors.o

COMMONPREF=sss_
TARGETS=libseh.a
LDIRT=$(TARGETS)


default install:$(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)
	$(AR) ccurl $(TARGETS) $(O_INTERFACES) $(O_EVENTS) $(O_MISC)

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


