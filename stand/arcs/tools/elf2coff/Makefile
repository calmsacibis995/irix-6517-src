# makefile to build elf2coff binary to run on the local machine.
#
include ${ROOT}/usr/include/make/commondefs

MKDEPFILE= Makedepend
LDIRT= ${OBJECTS} loadelf32.c loadelf64.c loader.c
CFILES= elf2coff.c loadelf64.c loadelf32.c loader.c 
LOADERDIR= ../../lib/libsk/lib/
TARGETS= elf2coff


LCDEFS=-D_STANDALONE -DMP -D_KERNEL -DELF_2_COFF
LCINCS=-I../../include -I../../lib/libsk/lib -I$(ROOT)/usr/include

default: ${TARGETS}

elf2coff: $(OBJECTS)
	$(CC) ${CSTYLE_N32_M3} $(PRODMODEL) $(CINCS) $(CDEFS) $(CVERSION) $(MKDEPOPT) $(CROSSENV) $(OBJECTS) -o $@

include ${COMMONRULES}

install: default

loadelf32.c:	$(LOADERDIR)/loadelf.c
	@touch $@
	@chmod u+w $@
	sed 's/ELFSIZE/32/g;s/PTRFMT//g' < $? > $@
	@chmod a-w $@

loadelf64.c:	$(LOADERDIR)/loadelf.c
	@touch $@
	@chmod u+w $@
	sed 's/ELFSIZE/64/g;s/PTRFMT/ll/g' < $? > $@
	@chmod a-w $@

loader.c: $(LOADERDIR)/loader.c
	@touch $@
	@chmod u+w $@
	cp $? $@
	@chmod a-w $@
