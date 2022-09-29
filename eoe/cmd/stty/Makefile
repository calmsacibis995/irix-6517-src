include ${ROOT}/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

TARGETS=stty

default: ${TARGETS}

include ${COMMONRULES}

install: ${TARGETS}
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/stty stty
