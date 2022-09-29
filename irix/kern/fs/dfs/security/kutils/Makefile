#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
# 
# 
# HISTORY
# $Log: Makefile,v $
# Revision 65.8  1999/05/21 18:53:49  lauriec
# Added build for files in security/utils.
#
# Revision 65.7  1999/02/03 22:44:25  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.6  1998/02/27 17:32:07  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.5  1998/02/27 16:16:05  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.4  1998/02/26 20:55:09  lmc
# Remove the setting of TARGET_FLAGS to 32 or 64 bit.  It gets these
# from the machdep.mk file.
#
# Revision 65.2  1997/10/20  19:48:30  jdoak
# Initial IRIX 6.4 code merge
#
# Revision 1.1.6.2  1996/03/09  23:29:54  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:45:15  marty]
#
# Revision 1.1.6.1  1995/12/08  17:46:16  root
# 	Submit OSF/DCE 1.2.1
# 
# 	HP revision /main/HPDCE02/1  1994/11/23  18:02 UTC  sommerfeld
# 	checkpoint (kernel & misc makefile cleanup)
# 	[1994/11/07  21:29 UTC  sommerfeld  /main/sommerfeld_mothmk_1/1]
# 
# Revision 1.1.4.1  1994/08/09  17:32:41  burati
# 	DFS/EPAC/KRPC/dfsbind changes
# 	[1994/08/09  17:00:44  burati]
# 
# Revision 1.1.2.2  94/07/22  15:49:25  rsarbo
# 	new makefile to build libksec.a which contains sec_cred interfaces
# 	for kernel use
# 
# $EndLog$
#
#
#
#  OSF DCE Version 1.1
#
#  NAME:
#
#      src/security/kutil/Makefile for SEC kernel runtime library
#
#  FACILITY:
#
#      Security 
#
#  ABSTRACT:
#
#  Kernel security library containing sec_cred() interfaces for doing
#  delegation in the DFS server.
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.8 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/security/kutils
.PATH:$(KPATH) $(KPATH)/../utils

KCINCS=		-I$(KPATH)/../h \
		-I$(KPATH)/../idl \
		-I$(ROOT)/usr/include \
                -I$(ROOT)/usr/include/dce/ker \
		-I$(ROOT)/usr/include/dcedfs

LIB_NAME=	libksec.a

CFILES=		sec_kmalloc.c

# files from ../utils
CFILES+=        sec_cred.c \
                sec_cred_internal.c \
                sec_encode.c \
                id_encode_cstub.c \
                sec_login_encode_cstub.c \
                authz_encode_cstub.c \
                restrictions.c

OBJS=           $(CFILES:.c=.o)
LIB_FILES=	$(OBJS)

$(LIB_NAME):$(LIB_NAME)($(LIB_FILES)) MAKELIB

TARGETS=	$(LIB_NAME)

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!

