SLDEPTH= ..
include ${SLDEPTH}/libsldefs

#
# Set up to build machine dependent objects
OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

_XX=\#
64_XX=\#
$(COMPILATION_MODEL)_XX=

ASFILES=
CFILES=

$(_XX)ASFILES= callarcsprom.s clientasm.s
$(_XX)CFILES= dload.c loader.c loadelf32.c loadelf64.c membig.c memory.c \
	relocate.c relocate64.c range_check.c stubs.c

K64PROM32=
LLCDEFS=-DLIBSL -U_K64PROM32
LLCINCS=-I$(INCLDIR)/cmplrs
ASDEFS=-DLIBSL -U_K64PORM32

LLLDIRT=loadelf32.c loadelf64.c
include $(LIBSLRULES)

# generate 32 and 64 bit versions of the elf loader
# the chmods are to discourage editing of the generated files
# the touch is to keep the first chmod from failing the first time

LOADELFDIR=../../libsk/lib
loadelf32.c:	$(LOADELFDIR)/loadelf.c
	@touch $@
	@chmod u+w $@
	sed -f sedfile32 < $(LOADELFDIR)/loadelf.c > $@
	@chmod a-w $@

loadelf64.c:	$(LOADELFDIR)/loadelf.c
	@touch $@
	@chmod u+w $@
	sed -f sedfile64 < $(LOADELFDIR)/loadelf.c > $@
	@chmod a-w $@

