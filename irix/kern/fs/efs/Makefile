#!smake
#
# Makefile for the SGI IRIS-4D Extent File System
#
# $Revision: 3.29 $
DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/fs/efs
.PATH:$(KPATH)

EFS_NAME=efs.a
EFS_CSRCS=efs_bitmap.c efs_bmap.c efs_dalloc.c efs_dir.c \
	  efs_ialloc.c efs_iget.c efs_inode.c efs_subr.c \
	  efs_vfsops.c efs_vnodeops.c
EFS_OBJS=$(EFS_CSRCS:.c=.o)
$(EFS_NAME):$(EFS_NAME)($(EFS_OBJS)) MAKELIB

QUOTA_NAME=quotas.o
QUOTA_CSRCS=quota.c quota_efs.c quota_scalls.c
QUOTA_OBJS=$(QUOTA_CSRCS:.c=.o)

MAC_NAME=mac_efs.o
MAC_CSRCS=mac_efs.c
MAC_OBJS=$(MAC_CSRCS:.c=.o)

TARGETS=$(EFS_NAME) $(QUOTA_NAME) $(MAC_NAME)

LINTNAME=efs
CFILES=$(EFS_CSRCS) $(QUOTA_CSRCS) $(MAC_CSRCS)
HFILES=efs.h efs_clnt.h efs_dir.h efs_fs.h efs_ino.h efs_sb.h \
       efs_bitmap.h efs_inode.h

#
# Rules
#
include ${DEPTH}/kcommonrules

$(QUOTA_NAME): $(QUOTA_OBJS)
	${LDF} -r -o $@ ${QUOTA_OBJS}

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${EFS_NAME}
	${KINSTALL} ${MAC_NAME}
	${KINSTALL} -idb std.sw.quotas ${QUOTA_NAME}
	${KDEVINSTALL} ${TARGETS}

# XXX should install into /usr/include/efs
headers!
	$(INSTALL) -m 444 -F /usr/include/sys/fs $(HFILES)
