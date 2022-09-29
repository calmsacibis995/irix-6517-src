# ide elf makefile.

DIAGDEPTH= ..
include ${DIAGDEPTH}/Makedefs

LIBNAME=libideelf.a

CFILES= ide_elf.c ide_elf_cmds.c \
        $(TARGETDIR)/loadelfsymtab32.c $(TARGETDIR)/loadelfsymtab64.c \
        $(TARGETDIR)/loadelfdynamic32.c $(TARGETDIR)/loadelfdynamic64.c 

ASFILES=

LINCLUDES=-I.

YFLAGS=-d

default: $(OBJECTDIR) $(OBJECTS)

LLDIRT=$(TARGETDIR)/loadelfsymtab32.c $(TARGETDIR)/loadelfsymtab64.c

# generate 32 and 64 bit versions of the elf symbol table loader
# the chmods are to discourage editing of the generated files
# the touch is to keep the first chmod from failing the first time

$(TARGETDIR)/loadelfsymtab32.c: loadelfsymtab.c
	@touch $(TARGETDIR)/$@ 
	@chmod u+w $(TARGETDIR)/$@ 
	sed 's/ELFSIZE/32/g;s/PTRFMT//g' < $? > $(TARGETDIR)/$@ 
	@chmod a-w $(TARGETDIR)/$@ 

$(TARGETDIR)/loadelfsymtab64.c: loadelfsymtab.c
	@touch $(TARGETDIR)/$@ 
	@chmod u+w $(TARGETDIR)/$@ 
	sed 's/ELFSIZE/64/g;s/PTRFMT/ll/g' < $? > $(TARGETDIR)/$@ 
	@chmod a-w $(TARGETDIR)/$@ 

# generate 32 and 64 bit versions of the elf dynamic loader
# the chmods are to discourage editing of the generated files
# the touch is to keep the first chmod from failing the first time

$(TARGETDIR)/loadelfdynamic32.c: loadelfdynamic.c
	@touch $(TARGETDIR)/$@ 
	@chmod u+w $(TARGETDIR)/$@ 
	sed 's/ELFSIZE/32/g;s/PTRFMT//g' < $? > $(TARGETDIR)/$@ 
	@chmod a-w $(TARGETDIR)/$@ 

$(TARGETDIR)/loadelfdynamic64.c: loadelfdynamic.c
	@touch $(TARGETDIR)/$@ 
	@chmod u+w $(TARGETDIR)/$@ 
	sed 's/ELFSIZE/64/g;s/PTRFMT/ll/g' < $? > $(TARGETDIR)/$@ 
	@chmod a-w $(TARGETDIR)/$@ 

include ${DIAGDEPTH}/Makerules



