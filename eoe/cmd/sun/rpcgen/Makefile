#
# Makefile for rpcgen, the Sun RPC protocol compiler.
#
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

CFILES= rpc_clntout.c rpc_cout.c rpc_hout.c rpc_main.c rpc_parse.c rpc_scan.c \
	rpc_svcout.c rpc_tblout.c rpc_util.c
HFILES= rpc_parse.h rpc_scan.h rpc_util.h

LLDLIBS= 

TARGETS= rpcgen

default: ${TARGETS}

include ${COMMONRULES}

rpcgen: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

install: default
	${INSTALL} -F /usr/bin rpcgen
