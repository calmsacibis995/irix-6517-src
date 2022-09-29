#!smake
#ident $Revision: 1.11 $
include ../commondefs

CFILES=	efs_aino.c efs_bget.c efs_bmap.c efs_bput.c efs_cksum.c \
	efs_enter.c efs_extend.c efs_exts.c efs_iget.c efs_iput.c \
	efs_mknod.c efs_mount.c efs_update.c efs_write.c efs_mklost.c \
	efs_ocksum.c

TARGETS= libefs.a

include ../commonrules

libefs.a: ${OBJECTS}
	${AR} crl $@ ${OBJECTS}

install: all
