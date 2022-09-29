#
# Makefile for cmd/regress
#
#	"$Revisions$"

include $(ROOT)/usr/include/make/commondefs

SUBDIRS = lib tests man

.PRECIOUS: $(SUBDIRS)

CFILES = regressp.c
HFILES = regressp.h
TARGETS = regressp

IDB_TAG_SW = noship.sw.regress

COMMONPREF = regress_

default install $(COMMONTARGS): $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

include $(COMMONRULES)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(COMMONPREF)default: $(TARGETS)

$(COMMONPREF)install: $(COMMONPREF)default
	$(INSTALL) -F /usr/local/bin -idb $(IDB_TAG_SW) '$(TARGETS)'
	$(INSTALL) -dir -idb $(IDB_TAG_SW) /usr/regress 
	$(INSTALL) -dir -m 1777 -idb $(IDB_TAG_SW) /usr/regress/tmp
	$(INSTALL) -dir -m 1777 -idb $(IDB_TAG_SW) /usr/regress/save

$(TARGETS): $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

