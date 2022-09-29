#!smake
#**************************************************************************
# *									  *
# * 		 Copyright (C) 1986-1994 Silicon Graphics, Inc.		  *
# *									  *
# *  These coded instructions, statements, and computer programs  contain  *
# *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
# *  are protected by Federal copyright law.  They  may  not be disclosed  *
# *  to  third  parties  or copied or duplicated in any form, in whole or  *
# *  in part, without the prior written consent of Silicon Graphics, Inc.  *
# *									  *
# **************************************************************************/

#ident	"$Revision: 1.49 $"

BASELIB=libmalloc_32.a
ALTLIBS=libmalloc_32_ns.a \
	libmalloc_64.a libmalloc_64_ns.a \
	libmalloc_n32.a libmalloc_n32_ns.a

DSOEXPORTS=common.exports
DSOEXPORTS_32=common.exports
DSOEXPORTS_N32=common.exports
DSOEXPORTS_64=common.exports

include	$(ROOT)/usr/include/make/librootdefs
TARGETS+=llib-lmalloc.ln

LLDLIBS=-lc
#
# WARNING 85: definition of XXX preempts that definition in  ...
# WARNING 69: Definition of XXX in .so's interface section appears at least once before
#
LLDDSOOPTS_32=-woff 85,69
LLDDSOOPTS_N32=-woff 85,69
LLDDSOOPTS_64=-woff 85,69
LLDDSOOPTS=-LD_LAYOUT:segalign=0x4000

DSONAMEOPT=-soname libmalloc.so

default: $(TARGETS)

include $(LIBROOTRULES)

llib-lmalloc.ln:	llib-lmall.c
	$(LINT) -Y$(ROOT)/usr/include -vxd -Xa -omalloc -n llib-lmall.c

exports install! default
	$(INST_32) -m 444 -src libmalloc_32.a libmalloc.a
	$(I_32) $(INSTALL) -F /lib -m 444 -src libmalloc_32.so libmalloc.so
	$(I_32) $(INSTALL) -F ${INSTDIR} -lns ../../lib/libmalloc.so libmalloc.so
	$(INST_32_NS) -m 444 -idb "nostrip" -src libmalloc_32_ns.a libmalloc.a
	$(INSTALL) -F /usr/lib/lint -src llib-lmall.c llib-lmalloc
	$(INSTALL) -F /usr/lib/lint llib-lmalloc.ln
	$(INST_64) -m 444 -src libmalloc_64.a libmalloc.a
	$(INST_64) -m 444 -src libmalloc_64.so libmalloc.so
	$(INST_64_NS) -m 444 -idb "nostrip" -src libmalloc_64_ns.a libmalloc.a
	$(INST_N32) -m 444 -src libmalloc_n32.a libmalloc.a
	$(INST_N32) -m 444 -src libmalloc_n32.so libmalloc.so
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libmalloc_n32_ns.a libmalloc.a
