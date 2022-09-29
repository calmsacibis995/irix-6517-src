#!smake
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1986-1994 Silicon Graphics, Inc.		  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/
#
#ident	"$Revision: 1.57 $ $Author: jwag $"
#

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

SM=:

#
# A bit of a hack - rather than look at the lib name or OBJECT_STYLE
# we look at the actual flags being set. This way, if someone changes
# the generic "32" style to be mips2, all the right stuff happens
#
#if $(CSTYLE:M*-mips1*) != ""
SM=$(SETMAGIC)
#endif


#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES=
#elif $(LIBRARY) == "libc_nomp_32_M2_ns.a" || $(LIBRARY) == "libc_nomp_n32_M3_ns.a"
CFILES=mpstubs.c
#else
CFILES=usinit.c usconfig.c ulocks.c usemas.c \
	amalloc.c semlibc.c mp_def.c \
	sem_open.c \
	libmutexc.c taskcreate.c taskdes.c taskinit.c taskctl.c taskblock.c \
	barrier.c m_lib.c r4klocks.c \
	usmalloc.c libcthread.c libcthreadinit.c mplib.c \
	usflock.c ussystype.c uscasarena.c \
	pt.c

ASFILES = r4k.s r4k_sem.s libmutexs.s compare.s

#endif

include $(LIBLEAFRULES)

r4k.o libmutexs.o: $$(@:.o=.s)
	$(AS) -c $(ASFLAGS:S/-mips1/-mips2/) -o $@ $*.s
	$(SM) -mips1 $@
