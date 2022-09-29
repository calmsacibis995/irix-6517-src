#!smake
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
#ident	"$Revision: 1.31 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs
SUBDIR_CDEFS=-DBSD=43 -DALLOW_RES_DEBUG -DRESOLVSORT

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES=
#else
CFILES= base64.c \
	bindresvport.c \
	herror.c \
	hostreg.c \
	htonl.c \
	htons.c \
	ntohl.c \
	ntohs.c \
	getnetgrent.c \
	linkaddr.c \
	rcmd.c \
	res_data.c \
	res_comp.c \
	res_debug.c \
	res_init.c \
	res_mkquery.c \
	res_query.c \
	res_send.c \
	rexec.c \
	ruserpass.c \
	sysctl.c
#endif

include $(LIBLEAFRULES)
