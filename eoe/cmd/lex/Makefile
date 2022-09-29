#!smake

include	$(ROOT)/usr/include/make/commondefs

LCDEFS=-DESG_PKG=\"IRIX\" -DESG_REL=\"$(RELEASE)\"

YFILES=parser.y
CFILES = main.c sub1.c sub2.c header.c

LDIRT=y.tab.c

TARGETS=lex

default:$(TARGETS)

include $(COMMONRULES)

install: default
	 $(INSTALL) -F /usr/bin $(TARGETS)

lex:	$(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)
