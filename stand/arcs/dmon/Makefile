#
#	Top level makefile for debug prom
#
#	"$Revision: 1.1 $"


include $(ROOT)/usr/include/make/commondefs

### Define the product directories
SUBDIRS= libpdiag IP22

### This enures no attempt is made to rm these directories when
### the Makefile is interrupted.
.PRECIOUS: $(SUBDIRS)

### Targets the user created for the current directory need to have
### the COMMONPREF should be unique. A good choice is the current 
### directory name.
COMMONPREF=dmon

### $(COMMONTARGS) are tagets defined in commonrules. Namely:
### clobber, clean, rmtargets, depend, incdepend, fluff, tags.
###
default install $(COMMONTARGS):
	@for d in $(SUBDIRS); do \
		echo "====\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

### This target allows subdirectories to be made individually.
$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

include $(COMMONRULES)
