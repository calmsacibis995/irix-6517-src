#!smake
#ident $Revision: 1.9 $
include ${ROOT}/usr/include/make/commondefs

CFILES= efs_aino.c efs_bit.c efs_bmap.c efs_devconv.c efs_dirent.c \
	efs_enter.c efs_extend.c efs_iget.c efs_iput.c efs_mklost.c \
	efs_mknod.c efs_mount.c efs_nami.c efs_newreg.c efs_pr.c \
	efs_readalli.c efs_rwb.c efs_stuff.c efs_walk.c efs_write.c
TARGETS= libefs.a

LCOPTS=-fullwarn

all install: ${TARGETS}

include ${COMMONRULES}

libefs.a: ${OBJECTS}
	${AR} crl $@ $?
