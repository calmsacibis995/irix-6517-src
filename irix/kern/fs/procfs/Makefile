#!smake
#
# Makefile for fs/procfs.
#
#ident	"$Revision: 1.21 $"
DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/procfs
.PATH:$(KPATH)

HFILES=prdata.h procfs.h procfs64.h prsystm.h procfs_n32.h

MAC_NAME=mac_proc.o
MAC_CSRCS=mac_proc.c
MAC_OBJS=$(MAC_CSRCS:.c=.o)

PROC_NAME=proc.a
PROC_CSRCS=prioctl.c prmachdep.c prsubr.c prusrio.c prvfsops.c prvnops.c \
	priface.c irix5_priface.c priface2.c

#if defined(CELL_IRIX)
PROC_CSRCS+=prvfsops_cell.c pr_relocation.c procio.c
#endif 
PROC_OBJS=$(PROC_CSRCS:.c=.o)

TARGETS=$(PROC_NAME) $(MAC_NAME)
LINTNAME=procfs

$(PROC_NAME): $$@($(PROC_OBJS)) MAKELIB

include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default: $(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(TARGETS)
	${KDEVINSTALL} ${TARGETS}

headers!
	$(INSTALL) -m 444 -F /usr/include/procfs prdata.h procfs.h prsystm.h \
			procfs64.h procfs_n32.h
	$(INSTALL) -F /usr/include/sys -lns ../procfs/procfs.h procfs.h

priface.o: priface.c
	$(CC) $(CFLAGS) -D_INTERFACE_NATIVE -c $(.IMPSRC) && \
	$(LDF) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

irix5_priface.o: priface.c
	$(CC) $(CFLAGS) -I$(DEPTH)/fs/procfs -D_INTERFACE_IRIX5 \
	-c $(KPATH)/priface.c  -o $(.TARGET) && \
	$(LDF) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)
