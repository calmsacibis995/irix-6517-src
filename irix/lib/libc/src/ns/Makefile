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
#ident	"$Revision: 1.8 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs
#  SUBDIR_CINCS= -I../../../../include

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES=
#else
CFILES= ns_cache.c \
	ns_ethers.c \
	ns_group.c \
	ns_hosts.c \
	ns_list.c \
	ns_lookup.c \
	ns_netgroup.c \
	ns_networks.c \
	ns_passwd.c \
	ns_proto.c \
	ns_rpc.c \
	ns_services.c \
	ns_shadow.c
#endif

include $(LIBLEAFRULES)
