#!smake
#
# Makefile for the Ordinal's nsort examples directory
#
#	No compilation involved, just installs of demonstration files
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=benchmark.spec

default: $(TARGETS)

include $(COMMONRULES)

PROD=nsort
PROD_NAME=Nsort

$(PROD).params: Makefile
	rm -f $@
	$(ECHO) '# Default $(PROD_NAME) parameters for this system' > $@
	$(ECHO) '# -tempfile and -filesystem defaults are commonly placed here' >> $@

install: $(TARGETS) $(PROD).params
#	$(INSTALL) -idb dba.sw.$(PROD) -dir /usr $(PROD)
	$(INSTALL) -idb dba.sw.$(PROD) -F /usr/$(PROD) $(PROD).params
#	$(INSTALL) -idb dba.sw.$(PROD) -dir /usr/$(PROD) examples
	$(INSTALL) -idb dba.sw.$(PROD) -F /usr/$(PROD)/examples $(TARGETS)

