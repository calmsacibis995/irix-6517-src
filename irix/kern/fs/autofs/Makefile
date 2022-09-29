#!smake
#
# Makefile for the SGI IRIS-4D NFS port.
#
DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/autofs
.PATH:$(KPATH)

KCOPTS=-fullwarn -D_IRIX5

MAC_NAME=mac_autofs.o
MAC_CSRCS=mac_autofs.c
MAC_OBJS=$(MAC_CSRCS:.c=.o)

CLIENT_NAME=autofs.a
CLIENT_CSRCS= auto_subr.c auto_vfsops.c auto_vnops.c autofs_icrash.c
CLIENT_OBJS=$(CLIENT_CSRCS:.c=.o)
$(CLIENT_NAME):$(CLIENT_NAME)($(CLIENT_OBJS)) MAKELIB

NONALTTARGETS=$(CLIENT_NAME) $(MAC_NAME)
TARGETS=$(NONALTTARGETS) 

LINTNAME=autofs
CFILES=$(CLIENT_CSRCS)
HFILES=autofs.h autofs_prot.h

#
# Rules
#
include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(CLIENT_NAME) $(MAC_NAME)
	$(KDEVINSTALL) $(NONALTTARGETS)


headers!
	$(INSTALL) -m 444 -F /usr/include/sys/fs $(HFILES)


autofs_icrash.o: autofs_icrash.c
	$(CCF) -g3 -c $(JALR) $(KPATH)/autofs_icrash.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)



