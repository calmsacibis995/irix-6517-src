#!smake
#
# Makefile for the Cell File System
#

DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/cfs
.PATH:$(KPATH)

CFS_NAME=cfs.a
CFS_CSRCS=dvsubr.c dvnsubr.c dcvnops.c dsvnops.c dsvn.c dcvn.c \
	dvfs.c dcvfsops.c dsvfsops.c dcvfs.c dcvfssubr.c dsvfs.c dsvfssubr.c \
	cfs_relocation.c
CFS_OBJS=${CFS_CSRCS:.c=.o}

CFS_IDLS=dsvn.idl dcvn.idl dsvfs.idl dcvfs.idl
CFS_CSRCS+=$(CFS_IDLS:S/^/I_/g:.idl=_stubs.c)
KERNIDLFILES=$(CFS_IDLS)

$(CFS_NAME):$(CFS_NAME)($(CFS_OBJS)) MAKELIB

CFS_IDBG_NAME=cfsidbg.a
CFS_IDBG_CSRCS=cfs_idbg.c
CFS_IDBG_OBJS=${CFS_IDBG_CSRCS:.c=.o}
$(CFS_IDBG_NAME):$(CFS_IDBG_NAME)($(CFS_IDBG_OBJS)) MAKELIB

TARGETS=$(CFS_NAME) $(CFS_IDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=cfs
CFILES=$(CFS_CSRCS)

include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default: $(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(TARGETS)
	${KDEVINSTALL} ${TARGETS}

headers!
	$(INSTALL) -m 444 -F /usr/include/cfs $(HFILES)

cfs_idbg.o: cfs_idbg.c
	$(CCF) -c $(JALR) -G 0 $*.c
