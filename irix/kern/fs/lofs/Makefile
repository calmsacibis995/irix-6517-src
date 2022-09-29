#!smake
#
# Makefile for the SGI IRIS-4D NFS port.
#
#ident	"$Revision: 1.3 $"
DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/lofs
.PATH:$(KPATH)

#KCDEFS= -DLODEBUG
KCOPTS=-fullwarn -D_IRIX5

CLIENT_NAME=lofs.a
CLIENT_CSRCS= lofs_subr.c lofs_vfsops.c lofs_vnops.c lofs_icrash.c
CLIENT_OBJS=$(CLIENT_CSRCS:.c=.o)
$(CLIENT_NAME):$(CLIENT_NAME)($(CLIENT_OBJS)) MAKELIB

NONALTTARGETS=$(CLIENT_NAME)
TARGETS=$(NONALTTARGETS)

LINTNAME=lofs
CFILES=$(CLIENT_CSRCS)
HFILES=lofs_node.h lofs_info.h

#
# Rules
#
include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(CLIENT_NAME)
	$(KDEVINSTALL) $(NONALTTARGETS)


headers!
	$(INSTALL) -m 444 -F /usr/include/sys/fs $(HFILES)

lofs_icrash.o: lofs_icrash.c
	$(CCF) -g3 -c $(JALR) $(KPATH)/lofs_icrash.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

