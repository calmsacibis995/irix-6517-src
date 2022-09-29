#!smake
# $Revision: 1.12 $

include $(ROOT)/usr/include/make/commondefs

LCDEFS	=
LCOPTS	=
LLDOPTS =
LLDLIBS	= -ll

IDBTAG= -idb std.sw.tcp

HELPFILE=nslookup.help

CFILES	=debug.c getinfo.c list.c main.c send.c skip.c subr.c nsap_addr.c res_debug.c base64.c
LFILES	=commands.l

TARGETS	=nslookup

default all:$(TARGETS)

include $(COMMONRULES)

install:all
	$(INSTALL) $(IDBTAG) -F /usr/sbin "$(TARGETS)"
	$(INSTALL) $(IDBTAG) -m 444 -F /usr/share/misc "$(HELPFILE)"

nslookup: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
