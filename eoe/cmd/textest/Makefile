#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=dptex 
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LLDLIBS=-ldpipe -lX11 -lGL -lm
#LLDLIBS= -lX11 -lGL -lm

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/share/src/irix/examples/dpipe/src -m 444 -src dptex.c
