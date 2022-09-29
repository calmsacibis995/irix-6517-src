#
# Makefile for rpc header file installation.
#
include $(ROOT)/usr/include/make/commondefs

PUBLIC_HFILES=	\
	auth.h auth_des.h auth_sys.h auth_unix.h \
	clnt.h clnt_soc.h \
	des_crypt.h errorhandler.h key_prot.h nettype.h \
	pmap_clnt.h pmap_prot.h pmap_rmt.h \
	raw.h rpc.h rpc_com.h rpc_msg.h rpcb_clnt.h rpcb_prot.h \
	svc.h svc_auth.h svc_soc.h \
	types.h xdr.h svc_mt.h

HPUT= $(INSTALL) -m 444 -F /usr/include/rpc

default: install

include $(COMMONRULES)

headers: install

install:
	$(HPUT) $(PUBLIC_HFILES)
