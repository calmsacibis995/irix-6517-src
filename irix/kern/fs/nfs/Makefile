#!smake
#
# Makefile for the SGI IRIS-4D NFS port.
#
#ident	"$Revision: 3.69 $"
DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/nfs
.PATH:$(KPATH)

KCOPTS=-D_IRIX5 -DNSD

KCINCS= -I$(TOP)/bsd

#
# NFS client bits
#
CLIENT_NAME=nfs.a
CLIENT3_NAME=nfs3.a
CLIENT_CSRCS= auth_kern.c auth_none.c \
	authunix_prot.c bootparam_xdr.c \
	clnt_kudp.c clnt_perror.c \
	nfs_tcp.c \
	nlm_prot.c mountxdr.c \
	nfs_compat.c nfs_rmount.c nfs_rnode.c nfs_subr.c \
	nfs_vfsops.c nfs_vnodeops.c nfs_xdr.c \
	pmap_kgetport.c pmap_prot.c \
	rpc_callmsg.c rpc_prot.c rpccommondata.c \
	bds.c subr_kudp.c nlm_debug.c \
	xdr.c xdr_array.c xdr_mbuf.c xdr_mem.c xdr_reference.c nfs3_xdr.c \
	xattr_xdr.c	\
	nfs_icrash.c \
	nsd_subr.c
CLIENT3_CSRCS= nfs_client.c nfs_cnvt.c \
	nfs3_rnode.c \
	nfs3_vnops.c nfs3_vfsops.c xattr_clnt.c xattr_subr.c
CLIENT_OBJS=$(CLIENT_CSRCS:.c=.o)
CLIENT3_OBJS=$(CLIENT3_CSRCS:.c=.o)
$(CLIENT_NAME):$(CLIENT_NAME)($(CLIENT_OBJS)) MAKELIB
$(CLIENT3_NAME):$(CLIENT3_NAME)($(CLIENT3_OBJS)) MAKELIB

#
# kernel RPC service support
#
SVCKUDP_NAME=svckudp.a
SVCKUDP_CSRCS=svc_kudp.c nfs_svc.c svc_ktcp.c
SVCKUDP_OBJS=$(SVCKUDP_CSRCS:.c=.o)
$(SVCKUDP_NAME):$(SVCKUDP_NAME)($(SVCKUDP_OBJS)) MAKELIB

#
# NFS server bits
#
SERVER_NAME=snfs.a
SERVER_CSRCS=nfs_export.c nfs_server.c \
	svc_auth.c nfs_common.c nlm_svc.c nlm_share.c xattr_server.c
SERVER_OBJS=$(SERVER_CSRCS:.c=.o)
$(SERVER_NAME):$(SERVER_NAME)($(SERVER_OBJS)) MAKELIB

#
# lock daemon
#
LOCKD_NAME=lockd.o
LOCKD_CSRCS=klm_lockmgr.c lockd_server.c nlm_async.c sm_monitor.c nlm_rpc.c
LOCKD_OBJS=$(LOCKD_CSRCS:.c=.o)
$(LOCKD_NAME):	$(LOCKD_OBJS)
	$(LDF) -r -o $@ $(LOCKD_OBJS)

ACL_NAME=acl_nfs3.o
ACL_CSRCS=acl_nfs3.c
ACL_OBJS=$(ACL_CSRCS:.c=.o)

MAC_NAME=mac_nfs3.o
MAC_CSRCS=mac_nfs3.c
MAC_OBJS=$(MAC_CSRCS:.c=.o)

#
# idbg module
#
NFSIDBG_NAME=nfsidbg.a
NFSIDBG_CSRCS=nfsidbg.c
NFSIDBG_OBJS=${NFSIDBG_CSRCS:.c=.o}
$(NFSIDBG_NAME):$(NFSIDBG_NAME)($(NFSIDBG_OBJS)) MAKELIB


NONALTTARGETS=$(CLIENT_NAME) $(SERVER_NAME) $(ACL_NAME) $(MAC_NAME) $(CLIENT3_NAME) $(LOCKD_NAME) $(SVCKUDP_NAME) $(NFSIDBG_NAME)
TARGETS=$(NONALTTARGETS)

LINTNAME=nfs
CFILES=$(CLIENT_CSRCS) $(SERVER_CSRCS) $(CLIENT3_CSRCS) $(LOCKD_CSRCS) $(SVCKUDP_CSRCS)
HFILES=export.h nfs.h nfs_clnt.h rnode.h nfs_stat.h bds.h

#
# Rules
#
include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(LBOOTPOSTOP) -idb nfs.sw.nfs $(SERVER_NAME)
	$(KINSTALL) $(LBOOTPOSTOP) -idb nfs.sw.nfs $(LOCKD_NAME)
	$(KINSTALL) $(SVCKUDP_NAME)
	$(KINSTALL) $(CLIENT_NAME)
	$(KINSTALL) $(LBOOTPOSTOP) -idb nfs3.sw.nfs3 $(CLIENT3_NAME)
	$(KINSTALL) $(ACL_NAME)
	$(KINSTALL) $(MAC_NAME)
	$(KINSTALL) $(NFSIDBG_NAME)
	$(KDEVINSTALL) $(NONALTTARGETS)

# XXX should install into /usr/include/nfs
headers!
	$(INSTALL) -m 444 -F /usr/include/sys/fs $(HFILES)

nfsidbg.o: nfsidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/nfsidbg.c

nfs_icrash.o: nfs_icrash.c
	$(CCF) -g3 -c $(JALR) $(KPATH)/nfs_icrash.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

