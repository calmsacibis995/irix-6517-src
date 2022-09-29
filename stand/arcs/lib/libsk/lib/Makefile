#
# Makefile libsk/lib
#
# This directory contains the core of the standalone kernel.
# Files in this directory are machine independent
#
# $Revision: 1.47 $

SKDEPTH= ..
include ${SKDEPTH}/libskdefs
include $(DEPTH)/netdbxdefs

LLCDEFS=-D${CPUBOARD} $(NETDBXFLAGS)

#
# Set up to build machine dependent objects
OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

ASFILES=swbcopy.s tlb.s clientasm.s
CFILES=	addrspace.c alarm.c conf.c config.c dload.c errputs.c fault.c	\
	genpda.c hook.c loader.c memory.c parse.c arcsio.c saio.c	\
	setenv.c signal.c spb.c regdesc.c tty.c	\
	memops.c skutils.c loadelf32.c loadelf64.c	\
	range_check.c relocate.c relocate64.c tile.c

LLLDIRT = loadelf32.c loadelf64.c
include $(LIBSKRULES)

# generate 32 and 64 bit versions of the elf loader
# the chmods are to discourage editing of the generated files
# the touch is to keep the first chmod from failing the first time

loadelf32.c:	loadelf.c
	@touch $@
	@chmod u+w $@
	sed 's/ELFSIZE/32/g;s/PTRFMT//g' < $? > $@
	@chmod a-w $@

loadelf64.c:	loadelf.c
	@touch $@
	@chmod u+w $@
	sed 's/ELFSIZE/64/g;s/PTRFMT/ll/g' < $? > $@
	@chmod a-w $@

