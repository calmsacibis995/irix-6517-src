#!smake
#
# Makefile for fs/hwgfs.
#
#ident	"$Revision: 1.2 $"
DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/hwgfs
.PATH:$(KPATH)

MAC_NAME=mac_hwg.o
MAC_CSRCS=mac_hwg.c
MAC_OBJS=$(MAC_CSRCS:.c=.o)

ACL_NAME=acl_hwg.o
ACL_CSRCS=acl_hwg.c
ACL_OBJS=$(ACL_CSRCS:.c=.o)

HWGFS_NAME=hwgfs.a
HWGFS_CSRCS=hwgvnops.c hwgvfsops.c hwgsubr.c
HWGFS_OBJS=$(HWGFS_CSRCS:.c=.o)

TARGETS=$(HWGFS_NAME) $(MAC_NAME) $(ACL_NAME)
LINTNAME=hwgfs

$(HWGFS_NAME): $$@($(HWGFS_OBJS)) MAKELIB

include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default: $(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(TARGETS)
	${KDEVINSTALL} ${TARGETS}
