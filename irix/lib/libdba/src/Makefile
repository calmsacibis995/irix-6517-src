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
#ident	"$Revision: 1.1 $ $Author: wombat $"
#

DEPTH=.
include $(ROOT)/usr/include/make/libleafdefs

CFILES=kio.c kinit.c ksuspend.c kcancel.c kread.c kwrite.c kerror.c \
	 kreturn.c klio.c kfsync.c khold.c
SUBDIR_COPTS=-I../../libc/src/aio -I../../libc/inc

include $(LIBLEAFRULES)
