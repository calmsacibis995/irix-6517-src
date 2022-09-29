#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
#
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.8  1999/02/23 15:24:09  gwehrman
# Added px.h to HFILES for exporting.
#
# Revision 65.7  1999/02/04 19:19:43  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.6  1998/02/27 17:31:53  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.5  1998/02/27 16:15:55  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.4  1998/02/26 20:54:39  lmc
# Remove the setting of TARGET_FLAGS to 32 or 64 bit.  It gets these
# from the machdep.mk file.
#
# Revision 65.1  1997/10/20  19:17:29  jdoak
# *** empty log message ***
#
# Revision 1.1.528.1  1996/10/02  18:12:40  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:45:24  damon]
#
# Revision 1.1.523.2  1994/06/09  14:17:32  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:29:40  annie]
# 
# Revision 1.1.523.1  1994/02/04  20:27:08  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:16:40  devsrc]
# 
# Revision 1.1.521.1  1993/12/07  17:31:40  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  16:16:05  jaffe]
# 
# Revision 1.1.2.6  1993/01/21  14:53:44  cjd
# 	embedded copyright notice
# 	[1993/01/20  14:57:42  cjd]
# 
# Revision 1.1.2.5  1992/11/24  18:24:31  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:16:42  bolinger]
# 
# Revision 1.1.2.4  1992/10/27  21:25:20  jaffe
# 	Transarc delta: mcinerny-ot5511-global-module-level-compiler-switches 1.1
# 	  Selected comments:
# 	    Want to be able to control module-specific CFLAGS via the machdep file.
# 	    Understand px_KERNELCFLAGS
# 	Transarc delta: mcinerny-ot5633-install-px_trace.h-properly 1.1
# 	  Selected comments:
# 	    px_trace.h was being installed in usr/lib, not usr/include/dcedfs
# 	    Add a px_trace.h_EXPDIR line.
# 	[1992/10/27  14:42:54  jaffe]
# 
# Revision 1.1.2.3  1992/09/25  18:51:31  jaffe
# 	Transarc delta: jess-ot5380-fix-cm-px-makefile 1.1
# 	  Selected comments:
# 	    The catalog files "cm/dfszcm.cat" and "px/dfszpx.cat" have not been installed.
# 	    Fix the problem.
# 	[1992/09/23  19:26:09  jaffe]
# 
# Revision 1.1.2.2  1992/09/15  13:16:21  jaffe
# 	Transarc delta: kazar-ot4921-basic-cmfx-events 1.1
# 	  Selected comments:
# 	    add some trace points, fix up bugs in logging package.
# 	    add trace points
# 	[1992/09/14  20:21:11  jaffe]
# 
# Revision 1.1  1992/01/19  02:56:17  devrcs
# 	Initial revision
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.8 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/px
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libpx.a
OBJ_CSRCS=	px_intops.c \
		px_init.c \
		px_subr.c \
		px_remove.c \
		px_repops.c \
		px_vlutils.c \
		px_moveops.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		px.h \
		px_trace.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

