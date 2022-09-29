#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.8  1999/02/04 19:19:47  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.7  1998/05/01 22:03:06  tdm
# Added dfszzl.cat.m to ILIST.
# Added dfszzl.cat.m_IDIR.
#
# Revision 65.6  1998/02/27  17:32:04  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.5  1998/02/27 16:16:03  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.4  1998/02/26 20:54:59  lmc
# Remove the setting of TARGET_FLAGS to 32 or 64 bit.  It gets these
# from the machdep.mk file.
#
# Revision 65.2  1997/12/16  17:05:51  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:18:12  jdoak
# *** empty log message ***
#
# Revision 1.1.541.1  1996/10/02  19:04:56  damon
# 	New DFS from Transarc
# 	[1996/10/01  19:00:10  damon]
#
# Revision 1.1.536.3  1994/06/09  14:26:57  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:38:49  annie]
# 
# Revision 1.1.536.2  1994/02/04  20:37:56  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:21:25  devsrc]
# 
# Revision 1.1.535.1  1993/12/07  17:39:43  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  18:26:27  jaffe]
# 
# Revision 1.1.2.4  1993/01/21  16:36:57  cjd
# 	embedded copyright notice
# 	[1993/01/20  18:30:59  cjd]
# 
# Revision 1.1.2.3  1992/11/24  20:46:49  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:43:08  bolinger]
# 
# Revision 1.1.2.2  1992/10/27  21:54:03  jaffe
# 	Transarc delta: comer-3013-zlc-file-handling 1.13
# 	  Selected comments:
# 	    In order to handle the ability to continue to read or write and open
# 	    file after that file has been unlinked, is addressed by open-for-delete
# 	    (OFD) tokens.  When a file is unlinked, the file exporter or glue code
# 	    tries to get this OFD token; but, the token is not granted until all
# 	    other open tokens have been returned or have expired.
# 	    This scheme has to be modified to work with fileset moves or a moved
# 	    zero-linkcount (deleted) file's storage will be recovered prematurely.
# 	    This delta contains the implementation for a zero-linkcount (ZLC) file
# 	    manager that sits in the core extension and manages ZLC files with its
# 	    own thread and as its own host object.
# 	    zlc manager makefile.  Builds libkzlc.a and installs zlc.h.
# 	    Fixed a problem with the tkc code where the cache entry reference
# 	    count was not being decremented (tkc_Release wasn't being called),
# 	    so vnodes were never being released (and storage not reclaimed).
# 	    1) Replaced a call to volreg_Lookup with one to VOL_VGET to fix a
# 	    deadlock problem.  The volreg_Lookup was unnecessary anyway.
# 	    2) Changed code to no longer try and reset SOURCE and TARGET flags
# 	    in volume structure.
# 	    3) Keep track of the number of items on the ZLC list.
# 	    4) Call the ZLC clean function when destroying a volume.
# 	    Was using the wrong fid when doing a VGET.
# 	    Was calling volreg_LookupExtended with an xcode = 1.  How foolish of
# 	    me!  xcode should be set to 2.
# 	    Fixed another fid problem and fixed a problem with returning bogus
# 	    tokens.
# 	    The fid passed from VOLOP_SETATTR did not contain a volume or
# 	    cell.  This is wrong....
# 	    In the event an asyncronous grant was granted for a file no longer in the
# 	    zlc list, an incorrect value was being returned to the token manager.
# 	    Since 1) the volume package makes calls to the zlc package, 2) stand-alone
# 	    episode includes the volume package, 3) stand-alone episode doesn't
# 	    require the functionality from this package, and 4) stand-alone episode
# 	    cannot link sdcelfs.ext because of references to zlc symbols, a stubs file
# 	    is created here.  The file is actually compiled from the libefs/<system>
# 	    Makefiles.
# 	    Need to hook zlc into build process.
# 	    link problems
# 	Transarc delta: mcinerny-ot5511-global-module-level-compiler-switches 1.1
# 	  Selected comments:
# 	    Want to be able to control module-specific CFLAGS via the machdep file.
# 	    Understand zlc_KERNELCFLAGS
# 	[1992/10/27  15:59:05  jaffe]
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
KPATH=$(TOP)/fs/dfs/file/zlc
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libkzlc.a
OBJ_CSRCS=	zlc_host.c zlc_mgr.c zlc_remove.c zlc_linkage.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		zlc.h zlc_trace.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

