#!smake

#ident	"$Revision: 1.3 $"

DEPTH=.
CFILES = mediaclient.c xdr_mc.c
LWOFF=,1209
LDIRT=testclient

include $(ROOT)/usr/include/make/libleafdefs
include $(LIBLEAFRULES)

default! testclient


CLIENT_OBJECTS = clientmain.o
CLIENT_LIBRARIES = $(TOP)$(LIBRARY)

testclient:	$(CLIENT_OBJECTS) $(CLIENT_LIBRARIES)
	$(CCF) $(CLIENT_OBJECTS) $(LDFLAGS) $(CLIENT_LIBRARIES) -o $@
