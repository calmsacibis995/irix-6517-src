#ident "$Revision: 1.11 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=nrm
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
CFILES=rm.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin -src nrm rm
	$(INSTALL) -F /usr/bin -lns ../../sbin/rm rm

$(TARGETS): ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@
