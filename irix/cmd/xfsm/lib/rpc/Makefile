#!smake -M
#ident "$Revision: 1.8 $"

DEPTH=../..
include $(DEPTH)/Makedefs

RPCGEN=rpcgen -I
LIB=../lib
LCINCS=-I../../include -I../../../xlv/include

CFILES = xfs_info_client.c xfs_rpc_defs_clnt.c \
	 xfs_rpc_defs_xdr.c xfs_rpc_defs_svc.c

HFILES= xfs_rpc_defs.h

LLDOPTS= -rpath /usr/opt/xfsm/lib
DEBUG = -g -DDEBUG
REGOPTS = -D_SGI_SOURCE 
LCOPTS = $(REGOPTS)
LDIRT = xfs_rpc_defs.h xfs_rpc_defs_clnt.c xfs_rpc_defs_xdr.c \
	xfs_rpc_defs_svc.c ../../include/xfs_rpc_defs.h

TARGETS = xfs_rpc_defs.h $(OBJECTS)

default: $(TARGETS)

include $(COMMONRULES)

xfs_rpc_defs_svc.c: xfs_rpc_defs.h
xfs_rpc_defs_xdr.c: xfs_rpc_defs.h
xfs_rpc_defs_clnt.c: xfs_rpc_defs.h

xfs_rpc_defs.h: xfs_rpc_defs.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -I xfs_rpc_defs.x
	cp xfs_rpc_defs.h ../../include/xfs_rpc_defs.h

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
