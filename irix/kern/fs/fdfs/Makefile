#!smake
#
# Makefile for fs/fdfs.
#
#ident	"$Revision: 1.8 $"
DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/fdfs
.PATH:$(KPATH)

FDFS_NAME=fd.a
FDFS_CSRCS=fdops.c
#if defined(CELL_IRIX)
FDFS_CSRCS+=fdops_cell.c
#endif 
FDFS_OBJS=$(FDFS_CSRCS:.c=.o)

MAC_NAME=mac_fd.o
MAC_CSRCS=mac_fd.c
MAC_OBJS=$(MAC_CSRCS:.c=.o)

TARGETS=$(FDFS_NAME) $(MAC_NAME)
LINTNAME=fdfs
CFILES=$(FDFS_CSRCS) $(MAC_CSRCS)

$(FDFS_NAME): $$@($(FDFS_OBJS)) MAKELIB

#
# Rules
#

include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default: $(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(FDFS_NAME)
	$(KINSTALL) $(MAC_NAME)
	${KDEVINSTALL} ${TARGETS}
