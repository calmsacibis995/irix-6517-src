include	$(ROOT)/usr/include/make/commondefs

TARGETS=pgun

LCDEFS=-g -mips2
LASDEFS= -mips2

CFILES=pgun.c
ASMFILES=pgunasm.s

default: $(TARGETS)

include $(COMMONRULES)

install: default
#	$(INSTALL) -F /usr/bin $(TARGETS)

pgun:	pgun.o	pgunasm.o
	$(CC) $(CCFLAGS) -o pgun pgun.o pgunasm.o
