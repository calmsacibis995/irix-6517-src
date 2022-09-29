include $(ROOT)/usr/include/make/commondefs

CVERSION=-cckr

CFILES= tar.c
TARGETS= tar
LCDEFS= -DRMT -DTRUSTEDIRIX
LLDLIBS= -lrmt
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
