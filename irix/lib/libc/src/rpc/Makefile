#!smake
#
# Makefile for the Sun RPC package.
#
#ident "$Revision: 1.29 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES=
#else
CFILES= auth_none.c auth_unix.c authunix_prot.c \
	clnt_generic.c clnt_perror.c clnt_raw.c clnt_simple.c \
	clnt_tcp.c clnt_udp.c get_myaddress.c mt_misc.c \
	netname.c openchild.c pmap_clnt.c pmap_getmaps.c pmap_getport.c \
	pmap_prot.c pmap_prot2.c pmap_rmt.c rpc_callmsg.c rpc_prot.c \
	rpccommondata.c rpcdtablesize.c svc.c svc_auth.c svc_auth_unix.c \
	svc_lbl.c svc_raw.c svc_run.c svc_simple.c svc_tcp.c svc_udp.c \
	xdr.c xdr_array.c xdr_float.c xdr_mem.c xdr_rec.c xdr_reference.c \
	xdr_stdio.c errorhandler.c
#endif

include $(LIBLEAFRULES)
