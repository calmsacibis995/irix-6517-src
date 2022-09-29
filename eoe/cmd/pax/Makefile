#!smake
#ident "$Revision: 1.1 $"

OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs

CFILES = ar_io.c ar_subs.c buf_subs.c cache.c cpio.c file_subs.c ftree.c\
	gen_subs.c options.c pat_rep.c pax.c sel_subs.c tables.c tar.c\
	tty_subs.c fts.c strmode.c 

TARGETS=pax

LCDEFS = -DNET2_STAT

LLDLIBS=
LLDOPTS=-Wl,-I,$(ROOTRLDNAME_N32)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)

pax:	$(OBJECTS) 
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)

