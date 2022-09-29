#!smake

include $(ROOT)/usr/include/make/commondefs

include $(COMMONRULES)

default all: 

install exports : default

	${INSTALL} -F /usr/NetVis/lib/mibs -m 0444 rfc2206.mib
	${INSTALL} -F /usr/NetVis/lib/mibs -m 0444 rfc2213.mib
	${INSTALL} -F /usr/lib/netman/mibs -lns /usr/NetVis/lib/mibs/rfc2206.mib rfc2206.mib
	${INSTALL} -F /usr/lib/netman/mibs -lns /usr/NetVis/lib/mibs/rfc2213.mib rfc2213.mib
