
include ../sctcommondefs

#LCDEFS    = -DDEBUG
LCINCS = -nostdinc -I$(ROOT)/usr/include -I$(ROOT)/usr/include/sss -I../include -I ../../include
LLDLIBS =  -nostdlib -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32 -lgen -L/usr/lib32/internal -lconfigmon -lhwconfig -lswconfig -lklib -lutil -lalloc -lssdb -lsgmtask

TARGETS = confupdt

default: $(TARGETS)

include $(COMMONRULES)

rpc.espd: $(OBJECTS) Makefile
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@ $(LIBS)

install: default
	$(INSTALL) -idb $(SSS_EOE) -F $(ETCPATH) $(TARGETS)
