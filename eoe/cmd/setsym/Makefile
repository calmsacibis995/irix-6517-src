#!smake
#ident	"$Revision: 1.15 $

include	$(ROOT)/usr/include/make/commondefs

ELF_CFILES= elfsubr.c readsym.c setsym_elf.c
ELF_OBJECTS= $(ELF_CFILES:.c=.o)

# Turn off setsym_coff
TARGETS= setsym setphdr_elf

default: $(TARGETS)

setsym_coff:	setsym.c
	$(CCF) setsym.c -o $@ $(LDFLAGS) -lmld

setsym:	$(ELF_OBJECTS)
	$(CCF) $(ELF_OBJECTS) $(LDFLAGS) -o $@ -ldwarf -lelf

setphdr_elf:	elfsubr.o setphdr_elf.o
	$(CCF) elfsubr.o setphdr_elf.o $(LDFLAGS) -o $@ -lelf

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/cpu/sysgen/root/usr/sbin \
			-lns ../../../../../sbin/setsym setsym
	$(INSTALL) -F /usr/sbin $(TARGETS)
