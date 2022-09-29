#ident	"cmd/wakeupat: $Revision: 1.1 $"

include	$(ROOT)/usr/include/make/commondefs

YFILES=parser.y
LFILES=lexer.l

TARGETS=wakeupat

default:$(TARGETS)

include $(COMMONRULES)

install: default 
	$(INSTALL) -F /usr/sbin -idb std.sw.usrenv -m 755 $(TARGETS)

$(TARGETS):	$(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)
