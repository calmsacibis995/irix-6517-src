#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=dpcp dpprio dpcp1
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LLDLIBS=-ldpipe -lprio

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/share/src/irix/examples/dpipe/src -m 444 -src dpcp.c dpcp.c	
	${INSTALL} -F /usr/share/src/irix/examples/dpipe/src -m 444 -src Makefile.gifts Makefile
