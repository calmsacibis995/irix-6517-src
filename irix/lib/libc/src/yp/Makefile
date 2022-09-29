#!smake
#
# Makefile for nis routines in /usr/lib/libc.a.
#
#ident "$Revision: 1.15 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

#
# Look for headers which live in lib/libc/yp in Sun's source tree
# in head/rpcsvc in our tree.
#
SUBDIR_CINCS= -I${INCLDIR}/rpcsvc

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES=
#else
CFILES= yp_all.c yp_bind.c yp_enum.c yp_master.c yp_match.c yp_order.c \
	yperr_string.c ypprot_err.c ypv1_xdr.c ypxdr.c ypmaint_xdr.c \
	yp_update.c ypupdate_xdr.c
#endif

include $(LIBLEAFRULES)
