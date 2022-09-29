#!smake
#ident  "$Revision: 1.2 $"

include	$(ROOT)/usr/include/make/commondefs

CFILES	= rpc_clntout.c rpc_cout.c rpc_hout.c rpc_main.c rpc_parse.c \
	rpc_scan.c rpc_svcout.c rpc_tblout.c rpc_util.c

TARGETS	= rpcgen_tli
#LCDEFS	= -cckr

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)

$(TARGETS): $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
