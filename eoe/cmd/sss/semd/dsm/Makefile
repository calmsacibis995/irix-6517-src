#!smake
#
#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

### these are directories in which to run rawidb phase
SUBDIRS= dsm_interfaces dsm_events dsm_misc dsm_rules dsm_tools
O_INTERFACES=dsm_interfaces/dsm_seh.o dsm_interfaces/dsm_ssdb.o 
O_EVENTS=dsm_events/dsm_init.o dsm_events/dsm_events.o dsm_events/dsm_main.o \
	 dsm_events/dsm_alarm.o
O_MISC= dsm_misc/dsm_children.o dsm_misc/dsm_config.o dsm_misc/dsm_dso.o \
	dsm_misc/exec_dso.o
O_RULES=dsm_rules/dsm_execute.o dsm_rules/dsm_parse.o dsm_rules/dsm_rules.o
COMMONPREF=sss_

$(COMMONTARGS):$(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)

default install:$(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)
	$(AR) ccurl libdsm.a $(O_INTERFACES) $(O_EVENTS) $(O_MISC) $(O_RULES)

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

### rule for ism phase
ism:$(_FORCE)
	cd build; $(MAKE) buildism

### rule for images phase
images:$(_FORCE)
	cd build; $(MAKE) buildimages 

