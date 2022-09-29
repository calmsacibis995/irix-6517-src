#!smake

#ident "$Revision: 1.10 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

# The following files implement functions named in the SVR4 gABI.
#
ABI_CFILES =\
auth_des.c	auth_none.c	auth_sys.c	authsys_prot.c \
clnt_bcast.c	clnt_dg.c	clnt_generic.c	clnt_perror.c \
clnt_raw.c	clnt_simple.c	clnt_vc.c \
key_call.c	netname.c	netnamer.c	publickey.c \
rpc_callmsg.c	rpc_prot.c	rpcb_clnt.c	secretkey.c \
svc.c		svc_dg.c	svc_generic.c	svc_raw.c \
svc_run.c	svc_simple.c	svc_vc.c	svcdesname.c \
xdr.c		xdr_array.c	xdr_float.c	xdr_refer.c \
xdr_mem.c	xdr_rec.c	xdr_stdio.c 

# The following files contain no functions named in the gABI,
# but do contain functions invoked by ABI functions.
#
HELPER_CFILES=\
authdes_prot.c	gethostname.c	key_prot.c	\
pmap_clnt.c	pmap_prot.c	port.c		rpc_comdata.c	\
rpc_generic.c	rpc_soc.c	rpcb_prot.c	rpcdname.c	\
rtime_tli.c	svc_auth.c	svc_auth_sys.c	svcauth_des.c	\
xcrypt.c 

# The following files contain functions used by SVR4 programs shipped
# with IRIX
USED_CFILES=\
ti_opts.c

# The following files are not required by the SVR4 gABI, and are not 
# required to satisfy externals from ABI functions.  They are made by 
# the SVR4 makefile however.
#
OTHER_CFILES=\
gethostent.c	getrpcent.c	inet_ntoa.c	openchild.c	xdr_sizeof.c \
getdname.c

# The following files are in the SVR4 source tree, but 
# are not made in the SVR4 make file.
#
UNUSED_CFILES= clnt_bsoc.c xdr_mbuf.c

CFILES	= $(ABI_CFILES) $(HELPER_CFILES) $(USED_CFILES)

SUBDIR_CDEFS=-DNO_IMPORT -D_SVR4_TIRPC -DPORTMAP -DSYSLOG

include $(LIBLEAFRULES)
