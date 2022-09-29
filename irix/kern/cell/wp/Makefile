#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/cell/wp
.PATH:$(KPATH)

WP_NAME=cell.a
WP_IDLS=wpds.idl wpss.idl wpsc.idl
WP_CSRCS=wpc_domain_mgr.c wps_domain_mgr.c \
	 wpc_domain_svr.c wps_domain_svr.c \
	 $(WP_IDLS:S/^/I_/g:.idl=_stubs.c)

WP_OBJS=${WP_CSRCS:.c=.o}
$(WP_NAME):$(WP_NAME)($(WP_OBJS)) MAKELIB

KERNIDLFILES=$(WP_IDLS)

TARGETS=$(WP_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=cell
CFILES=$(WP_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
