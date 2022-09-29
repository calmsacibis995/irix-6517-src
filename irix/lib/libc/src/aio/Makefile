#!smake
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1992, Silicon Graphics, Inc.	  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/
#
#ident	"$Revision: 1.15 $ $Author: jwag $"
#

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a" || \
    $(LIBRARY) == "libc_nomp_32_M2_ns.a" || \
    $(LIBRARY) == "libc_nomp_n32_M3_ns.a"
    # aio routines are in libabi for the ABI libs
CFILES=
#else
#if $(LIBRARY) != "libc_n32_M3.a" && \
   $(LIBRARY) != "libc_n32_M4.a" && \
   $(LIBRARY) != "libc_n32_M3_ns.a" && \
   $(LIBRARY) != "libc_n32_M4_ns.a" 

OLDCFILES=old_aio.c old_ainit.c old_asuspend.c old_acancel.c old_aread.c \
	old_awrite.c old_aerror.c  old_areturn.c old_lio.c 
SUBDIR_COPTS=-D__AIO_52_COMPAT__
#else
OLDCFILES=
#endif
#if $(OBJECT_STYLE:M32*:N32_ABI) != ""
CFILES64=ainit64.c asuspend64.c acancel64.c aread64.c awrite64.c aerror64.c \
	 areturn64.c lio64.c afsync64.c ahold64.c
#else
CFILES64=
#endif
CFILES=aio.c ainit.c asuspend.c acancel.c aread.c awrite.c aerror.c \
	 areturn.c lio.c afsync.c ahold.c $(OLDCFILES) $(CFILES64)
#endif

include $(LIBLEAFRULES)
