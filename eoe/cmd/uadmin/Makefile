#ident "$Revision: 1.10 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=uadmin
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /usr/sbin -ln ../../sbin/uadmin uadmin
	${INSTALL} -F /etc -lns ../sbin/uadmin uadmin

