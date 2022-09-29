#!smake
#
# Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
#
#	Makefile for smtlib.a
#
#	"$Revision: 1.5 $

DEPTH=..
include ${DEPTH}/Makedefs

TARGETS=libsmt.a
CFILES=	smt_asn1.c smt_mib.c smt_parse.c smt_snmp.c \
	smt_snmp_api.c smt_snmp_auth.c smt_snmp_clnt.c \
	smt_subr.c \
	sm_map.c sm_log.c sm_mib.c sm_addr.c sm_str.c \
	cf.c cf_sm.c cf_q.c cf_sanity.c \
	tlv.c tlv_sm.c tlv_get.c tlv_set.c tlv_pr.c \
	ma.c ma_str.c

default install: ${TARGETS}

include ${COMMONRULES}

${TARGETS}: ${OBJECTS}
	${AR} rl $@ $?
