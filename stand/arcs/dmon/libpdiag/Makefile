#
#	Top level makefile for debug prom
#
#	"$Revision: 1.1 $"


include $(ROOT)/usr/include/make/commondefs

DEPTH= .

### Define the product directories
SUBDIRS= mem r4k monitor

### This enures no attempt is made to rm these directories when
### the Makefile is interrupted.
.PRECIOUS: $(SUBDIRS)

### Targets the user created for the current directory need to have
### the COMMONPREF should be unique. A good choice is the current 
### directory name.
COMMONPREF=libpdiag

### $(COMMONTARGS) are tagets defined in commonrules. Namely:
### clobber, clean, rmtargets, depend, incdepend, fluff, tags.
###
default install $(COMMONTARGS):
	if test ! -d $(DEPTH)/$(PRODUCT).O; then \
		mkdir $(DEPTH)/$(PRODUCT).O; \
	fi ;

	@for d in $(SUBDIRS); do \
		echo "====\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

### This target allows subdirectories to be made individually.
$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

include $(COMMONRULES)
