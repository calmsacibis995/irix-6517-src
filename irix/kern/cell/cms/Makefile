#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/cell/cms
.PATH:$(KPATH)

CMS_NAME=cell.a
CMS_CSRCS=cms_daemon.c cms_mesg.c cms_membership.c \
	cms_comb_dynamic.c  cms_trace.c

CMS_IDLS=cms.idl
CMS_CSRCS+=${CMS_IDLS:S/^/I_/g:.idl=_stubs.c}
KERNIDLFILES=$(CMS_IDLS)
CMS_OBJS=${CMS_CSRCS:.c=.o}

$(CMS_NAME):$(CMS_NAME)($(CMS_OBJS)) MAKELIB

TARGETS=$(CMS_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=cell
CFILES=$(CMS_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
