#!smake
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1989 Silicon Graphics, Inc.	  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/
#ident	"$Revision: 1.7 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES=
#else
CFILES= BSDdoprnt.c \
	BSDfopen.c \
	BSDfprintf.c \
	BSDprintf.c \
	setbuffer.c \
	BSDsprintf.c \
	BSDvfprintf.c \
	BSDvprintf.c \
	BSDvsprintf.c
#endif

SUBDIR_CDEFS=-DBSDSTDIO

include $(LIBLEAFRULES)
