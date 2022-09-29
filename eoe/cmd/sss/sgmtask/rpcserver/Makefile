
include ../sctcommondefs

CFILES = xfrdata.c server.c deliverremoteSrvr.c getdataSrvr.c subscribeSrvr.c confupdtSrvr.c setauthSrvr.c hnameSrvr.c
LCDEFS    = -DUSEINETD 
#-DDEBUG
LCINCS = -nostdinc -I$(ROOT)/usr/include -I$(ROOT)/usr/include/sss -I../include -I ../../include
LLDLIBS = -nostdlib -leventmon -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32 -L/usr/lib32/internal -lssdb -lsgmtask
#../libsgmtask/libsgmtask_n32_M3.a 

TARGETS = rpc.espd

default: $(TARGETS)

include $(COMMONRULES)

rpc.espd: $(OBJECTS) Makefile
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@ $(LIBS)

install: default
	$(INSTALL) -idb $(SSS_EOE) -F $(ETCPATH) $(TARGETS)
