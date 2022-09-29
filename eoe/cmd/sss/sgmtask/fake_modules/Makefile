
include ../sctcommondefs
LCDEFS    = -DYYDEBUG=1 -DDEBUG
LLDLIBS   = -nostdlib -lgen -lssdb -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32 -L$(ROOT)/usr/lib32/internal -lsgmtask


TARGETS = subscr 

default: $(TARGETS)

include $(COMMONRULES)

install: default
#	$(INSTALL) -idb $(SSS_EOE) -F /usr/etc -src subscr sgmsubscribe

