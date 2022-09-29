#
include	$(ROOT)/usr/include/make/commondefs

CFILES= LR0.c allocate.c closure.c conflicts.c derives.c files.c \
	getargs.c getopt.c getopt1.c gram.c lalr.c lex.c main.c nullable.c \
	output.c print.c reader.c reduce.c symtab.c version.c warshall.c alloca.c

SIMPLE=bison.simple
HAIRY=bison.hairy
TARGETS=bison
LLDLIBS=-lw
LCOPTS=-DXPFILE=\"$(ROOT)/usr/lib/$(SIMPLE)\" \
		-DXPFILE1=\"$(ROOT)/usr/lib/$(HAIRY)\"

default: $(TARGETS)

include $(COMMONRULES)

bison: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)
	$(INSTALL) -F /usr/lib $(SIMPLE)
	$(INSTALL) -F /usr/lib $(HAIRY)
