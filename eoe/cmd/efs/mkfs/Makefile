#!smake
#ident $Revision: 1.13 $
include ../commondefs

CFILES= mkfs.c
TARGETS= mkfs_efs

LCINCS= -I../lib
LLDOPTS= -L../lib -Wl,-I,$(ROOTRLDNAME)
LLDLIBS= -lefs -ldisk

include ../commonrules

mkfs_efs: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

install: all
	${INSTALL} -F /sbin -idb "std.sw.unix mr" ${TARGETS}
