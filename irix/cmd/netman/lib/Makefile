#!smake
#
#	Makefile for libnetvis
#
#	$Revision: 1.40 $
#
LIBDEPTH=.
include ${LIBDEPTH}/Makedefs

#
# Alphabetized and grouped source lists.
#
CFILES= addrlist_xdr.c \
	cache.c \
	ds_big.c ds_byte.c ds_bytes.c ds_field.c ds_init.c ds_inline.c \
	ds_int.c ds_little.c ds_seek.c ds_seekbit.c \
	enum.c \
	ex_create.c ex_eval.c ex_operate.c ex_parse.c ex_string.c ex_subr.c \
	ex_test.c ex_type.c ex_xdr.c \
	exc_syslog.c exception.c \
	heap.c \
	histogram_xdr.c \
	help.c \
	index.c in_qual.c \
	netlook_xdr.c \
	netsnoop_xdr.c \
	packetbuf.c pb_decode.c pb_match.c \
	pr_compiler.c pr_conf.c pr_eval.c pr_nest.c pr_register.c pr_scope.c \
	pr_stubs.c pr_xdr.c \
	protoindex.c \
	pv_field.c pv_frame.c pv_hex.c pv_init.c pv_null.c pv_nullify.c \
	pv_printf.c pv_stdio.c \
	scope.c \
	sfs_alloc.c sfs_compile.c \
	sn_capture.c sn_errsnoop.c sn_init.c sn_local.c sn_match.c sn_null.c \
	sn_parse.c sn_remote.c sn_stats.c \
	sn_trace.c sn_xdr.c \
	snoopd_xdr.c \
	snoopstream.c \
	strings.c

#if ${IRIX} == "SunOS"
CFILES= addrlist_xdr.c \
	cache.c \
	ds_big.c ds_byte.c ds_bytes.c ds_field.c ds_init.c ds_inline.c \
	ds_int.c ds_little.c ds_seek.c ds_seekbit.c \
	enum.c \
	ex_create.c ex_eval.c ex_operate.c ex_parse.c ex_string.c ex_subr.c \
	ex_test.c ex_type.c ex_xdr.c \
	exc_syslog.c exception.c \
	heap.c \
	histogram_xdr.c \
	help.c \
	index.c in_qual.c \
	netlook_xdr.c \
	netsnoop_xdr.c \
	packetbuf.c pb_decode.c pb_match.c \
	pr_compiler.c pr_conf.c pr_eval.c pr_nest.c pr_register.c pr_scope.c \
	pr_stubs.c pr_xdr.c \
	protoindex.c \
	pv_field.c pv_frame.c pv_hex.c pv_init.c pv_null.c pv_nullify.c \
	pv_printf.c pv_stdio.c \
	res_comp.c \
	scope.c \
	sfs_alloc.c sfs_compile.c \
	sn_capture.c sn_errsnoop.c sn_init.c sn_match.c sn_null.c \
	sn_parse.c sn_remote.c sn_stats.c sn_sun.c \
	sn_trace.c sn_xdr.c \
	snoopd_xdr.c \
	snoopstream.c \
	strings.c \
	clnt_tcp.c errorhandler.c svc.c svc_tcp.c xdr_rec.c
#endif

TARGETS= ${LIBNAME} libnetman_64 Marker
SUBDIRS= license tokenring llc protocols appletalk decnet snmp sunrpc novell \
	 xns novell ftp smtp telnet osi sna netbios vines tsp X rlogin rcp \
         libnetman_64

COMMONPREF=lib

default ${COMMONTARGS}: ${COMMONPREF}$$@
	${SUBDIRS_MAKERULE}

install exports: default
	${INSTALL} -idb nostrip -F ${LIBINSTPATH} -m 444 ${LIBNAME}
	${INSTALL} -idb nostrip -F /usr/lib64 -m 444 -src libnetman_64/libnetman.a ${LIBNAME}

include ${LIBDEPTH}/Makerules

${COMMONPREF}default ${COMMONPREF}exports: ${LIBNAME} libnetman_64

${SUBDIRS}: ${_FORCE}
	cd $@; ${MAKE}

.PRECIOUS: ${SUBDIRS}
