
SUBDIRS=cpr cview ctest

include $(ROOT)/usr/include/make/commondefs

default:
	$(SUBDIRS_MAKERULE)

COMMONPREF=xxx

include $(COMMONRULES)

$(COMMONTARGS) install: $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)


$(COMMONPREF)install:
