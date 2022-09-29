#
# Makefile to build the x86 standalone products
# 
# $Revision: 1.1 $
# $Date: 1996/09/23 02:05:20 $
#
DEPTH=.
include commondefs
include $(PRODUCTDEFS)
DEF_CSTYLE=

#
# Allow additional directories to be built based on current product.
# See the stand/arcs/Makefile for an example of how to extend this
# for >1 directories.
#
SN0_X=\#
$(PRODUCT)_X=

$(SN0_X)DIRS= ffsc

EXPORTS_SUBDIRS=
COMMONPREF=stand

# targets hit by build group--use only EVERYPRODUCT
default install .DEFAULT:
	@if [ -z "$$PRODUCT" ] ; then \
		for prod in $(EVERYPRODUCT); do \
			PRODUCT=$$prod; \
			export PRODUCT; \
			$(MAKE) $@; \
		done; \
	else \
		DIRS=" ${DIRS}"; \
		if [ "$$DIRS" != "" ]; then \
			for d in $$DIRS; do \
				echo "====\tcd $$d; $(MAKE) $@"; \
				( cd $$d; ${MAKE} $@ );  \
			done; \
		else \
			echo "====\tnothing to make for PRODUCT $$PRODUCT"; \
		fi; \
	fi

# other things that can get EXTRAPRODUCT w/o harm
clean rmtargets clobber tags fluff:
	@if [ -z "$$PRODUCT" ] ; then \
		for prod in $(EVERYPRODUCT) $(EXTRAPRODUCT); do \
			PRODUCT=$$prod; \
			export PRODUCT; \
			$(MAKE) $@; \
		done; \
	else \
		DIRS="${DIRS} ${OPTDIRS}"; \
		if [ "$$DIRS" != "" ]; then \
			for d in $$DIRS; do \
				echo "====\tcd $$d"; \
				( cd $$d; ${MAKE} $@ );  \
			done; \
		else \
			echo "====\tnothing to do for PRODUCT $$PRODUCT"; \
		fi; \
	fi

include $(COMMONRULES)

$(COMMONTARGS): $(COMMONPREF)$$@

# Nothing to do for exports or headers
headers exports:




