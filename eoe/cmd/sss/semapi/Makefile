#!smake
#
#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

### these are directories in which to run rawidb phase
SUBDIRS= server 
O_SERVER=server/semapisrv.o
LDIRT=*.a
COMMONPREF=sss_

$(COMMONTARGS):$(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)

exports default install:$(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)
	$(AR) ccurl libsemapisrv.a $(O_SERVER)


include $(COMMONRULES)

### We are not overriding commonrules. for default and install.
$(COMMONPREF)exports:
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

