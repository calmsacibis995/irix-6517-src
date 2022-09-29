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
# Revision 65.7  1999/02/04 19:19:45  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.6  1998/02/27 17:31:59  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.5  1998/02/27 16:15:58  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.4  1998/02/26 20:54:46  lmc
# Remove the setting of TARGET_FLAGS to 32 or 64 bit.  It gets these
# from the machdep.mk file.
#
# Revision 65.1  1997/10/20  19:19:48  jdoak
# *** empty log message ***
#
# Revision 1.1.640.1  1996/10/02  18:47:14  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:48:47  damon]
#
# Revision 1.1.635.2  1994/06/09  14:22:24  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:33:53  annie]
# 
# Revision 1.1.635.1  1994/02/04  20:32:47  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:19:03  devsrc]
# 
# Revision 1.1.633.1  1993/12/07  17:35:39  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  17:16:07  jaffe]
# 
# Revision 1.1.5.3  1993/01/21  15:53:33  cjd
# 	embedded copyright notice
# 	[1993/01/20  17:36:08  cjd]
# 
# Revision 1.1.5.2  1992/11/24  19:46:35  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:30:45  bolinger]
# 
# Revision 1.1.2.2  1992/05/20  20:15:13  mason
# 	Transarc delta: bab-ot2978-tkm-multi-thread-tkm_test 1.14
# 	  Files modified:
# 	    OSF1: Makefile; libafs/RIOS: Makefile; tkc: tkc.c, tkc.h
# 	    tkc.ulib: Makefile
# 	    tkm: Makefile, tkm_blockingOps.c, tkm_fidHash.c
# 	    tkm: tkm_getToken.c, tkm_hostData.h, tkm_hostList.h
# 	    tkm: tkm_hostStubs.c, tkm_hostStubs.h, tkm_test.h
# 	    tkm: tkm_test.l, tkm_test.y, tkm_testops.c, tkm_tkcHost.c
# 	    tkm: tkm_tkcsetHost.c, tkm_tkmHost.c, tkm_tksetHost.c
# 	    tkm: tkm_tokens.c, tkm_tokens_private.h
# 	    tkset: Makefile, tkset.h; tkset.ulib: Makefile
# 	    token_test: Makefile, tktest_hostStubs.c, tktest_hostStubs.h
# 	    token_test: tktest_test.h, tktest_test.l, tktest_test.y
# 	    token_test: tktest_testops.c, tktest_tkcsetHost.c
# 	    token_test: tktest_tkmHost.c
# 	  Selected comments:
# 	    This delta implements the use of the thread-per-host model in tkm_test.
# 	    This first version is a check-point that just implements the use of
# 	    host operation structures (to be used as work queue elements).
# 	    This version of this delta has the real per-host threads and work queues
# 	    in it.  This is another check-point before restoring the use of
# 	    the hostops vectors in the testing code.
# 	    Hostops vectors now used in user-space test.  Added more support
# 	    for tkset/tkc testing.
# 	    Checkpoint before importing the real tkc/tkset code.
# 	    Pick up changes to tkm_test.y that hadn't been saved.
# 	    Buildable check-point that includes tkm-host host auditing.
# 	    Changed name of tkset library built in kernel-space to use
# 	    libk*.a convention.
# 	    last check-point before pulling tkm_test out of tkm directory.
# 	    This one seems to be passing all the basic tkm/tkc/tkset tests.
# 	    Moved testing program into token_test directory.
# 	    Fix bug in tkc host handling.
# 	    Add support for host yield operation.
# 	    Support for printing strings during execution and attempt to get
# 	    thread numbers.
# 	    Compensate for error in build 11 version of pthread.h
# 	    tkc change to get desired interleaving in user-space tests.
# 	    Locate the fix for pthread_getunique_np in a place where more
# 	    of the test program can use it, and protected with ifdefs for RIOS.
# 	[1992/05/20  15:37:03  mason]
# 
# Revision 1.1  1992/01/19  02:57:45  devrcs
# 	Initial revision
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.7 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/tkset
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libktkset.a
OBJ_CSRCS=	tkset.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		tkset.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

