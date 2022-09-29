#!smake
# **************************************************************************
# *                                                                        *
# *              Copyright (C) 1996 Silicon Graphics, Inc.                 *
# *                                                                        *
# *  These coded instructions, statements, and computer programs  contain  *
# *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
# *  are protected by Federal copyright law.  They  may  not be disclosed  *
# *  to  third  parties  or copied or duplicated in any form, in whole or  *
# *  in part, without the prior written consent of Silicon Graphics, Inc.  *
# *                                                                        *
# **************************************************************************

#ident	"$Revision: 1.13 $"

BASELIB=libpthread_32.a
ALTLIBS=libpthread_n32_M3.a \
	libpthread_64.a

DSOEXPORTS=common.exports
DSOEXPORTS_32=common.exports
DSOEXPORTS_N32=common.exports
DSOEXPORTS_64=common.exports

include $(ROOT)/usr/include/make/librootdefs

DSONAMEOPT	= -soname libpthread.so
LLDLIBS		= -lc
LLDDSOOPTS	= -init _SGIPT_init1 -woff 134
HDRINST		= $(INSTALL) -F $(INSTINC) -m 444 -idb "noship"

default: $(TARGETS)

include $(LIBROOTRULES)

install!headers exports

exports!default libo32 libn32 libn64

headers!
	$(HDRINST) -src inc/common.h	pt/common.h
	$(HDRINST) -src inc/mtx.h	pt/mtx.h
	$(HDRINST) -src inc/mtxattr.h	pt/mtxattr.h
	$(HDRINST) -src inc/pt.h	pt/pt.h
	$(HDRINST) -src inc/ptdbg.h	pt/ptdbg.h
	$(HDRINST) -src inc/q.h		pt/q.h
	$(HDRINST) -src inc/sys.h	pt/sys.h
	$(HDRINST) -src inc/vp.h	pt/vp.h

libo32!	libpthread_32.so
	$(INST_32) -m 444 -src libpthread_32.so libpthread.so

libn32!	libpthread_n32_M3.so
	$(INST_N32) -m 444 -src libpthread_n32_M3.so libpthread.so

libn64!	libpthread_64.so
	$(INST_64) -m 444 -src libpthread_64.so libpthread.so
