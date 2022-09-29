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
# Revision 65.5  1999/05/04 19:19:32  gwehrman
# Added osi_wait.h to the list of HFILES exported.  Fix for bug 691629.
#
# Revision 65.4  1999/02/04 19:19:43  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.3  1998/12/04 17:26:37  bobo
# O32 build changes
#
# Revision 65.2  1997/12/16  17:05:38  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:17:41  jdoak
# *** empty log message ***
#
# Revision 1.1.172.1  1996/10/02  17:58:08  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:43:48  damon]
#
# Revision 1.1.167.4  1994/07/13  21:31:19  devsrc
# 	Merged with bl-10
# 	[1994/06/28  16:21:34  devsrc]
# 
# 	Added line for tom_LIBS (needed only on RIOS).
# 	[1994/04/07  18:19:52  mbs]
# 
# 	Merged with changes from 1.1.169.1
# 	[1994/04/04  18:11:33  mbs]
# 
# 	Added osi_alloc.h to INCLUDES and added tom to PROGRAMS.
# 	[1994/04/04  18:03:31  mbs]
# 
# 	Added ILIST and IDIR
# 	[1994/03/08  21:20:08  mckeen]
# 
# Revision 1.1.167.3  1994/06/09  14:15:28  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:27:58  annie]
# 
# Revision 1.1.167.2  1994/02/04  20:24:37  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:15:40  devsrc]
# 
# Revision 1.1.166.1  1993/12/07  17:29:48  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  16:04:09  jaffe]
# 
# Revision 1.1.7.7  1993/01/21  14:50:17  cjd
# 	embedded copyright notice
# 	[1993/01/20  14:52:13  cjd]
# 
# Revision 1.1.7.6  1992/12/10  22:27:26  marty
# 	Upgrade DFS to OSF/1 1.1.1.  [OT defect 5236]
# 	[1992/12/10  22:02:04  marty]
# 
# Revision 1.1.7.5  1992/11/24  18:22:15  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:13:05  bolinger]
# 	Revision 1.1.11.2  1992/11/30  23:12:28  marty
# 	OSF 1.1.1 port
# 
# Revision 1.1.7.4  1992/10/13  18:11:44  marty
# 	OSC1.1.1 upgrade.  Changes are osc1.0.4 compatible
# 	[1992/10/13  17:11:44  marty]
# 
# Revision 1.1.10.2  1992/10/06  15:46:21  garyf
# 	 Added osi_user_uio.h
# 	[1992/10/06  15:45:50  garyf]
# 
# Revision 1.1.7.3  1992/10/02  21:21:41  toml
# 	Added osi_sysconfig.h, removed osi_user_newProc.o
# 	[1992/10/02  19:24:42  toml]
# 
# Revision 1.1.7.2  1992/08/31  20:24:03  jaffe
# 	Transarc delta: bab-ot2604-tkm-gc-impl 1.27
# 	  Selected comments:
# 	    Had to make another checkpoint so I could import another delta.
# 	    Checkpoint version that passes basic user-space (non-regression - no real
# 	    exercise of the gc, yet) tests.
# 	    Removed subset bug fix in order to put it into the re-org delta.
# 	    NEVER IMPORT THIS VERSION OF THIS DELTA!
# 	    Re-establish the token subset bug fix in this delta.
# 	    Need to import another delta.
# 	    Another check-point version, this one in preparation for the one that
# 	    removes tkm_Alloc.  This version passes basic user-space tests of garbage
# 	    collection and asynchronous revocation.
# 	    Another check-point.  This one removes tkm_Alloc (and, hence,
# 	    tkm_memory.[ch]) from the token manager, since it doesn't really gain us
# 	    that much, especially now that token cells are managed as an internal list.
# 	    Another buildable checkpoint that passes basic periodic gc tests.
# 	    Implemented a routine to be used as a "try waiting tokens" thread,
# 	    along the same lines as the garbage collection thread.
# 	    Add exception catching (if activated) to periodic try waiting routine,
# 	    and make tkm panic/exit if it can't get memory from osi_Alloc;
# 	    Fix stupid compilation bug left in in debugging a couple of versions ago.
# 	    Implementation of token manager control afs_syscall.
# 	    (Untested check-point...probably doesn't even compile)
# 	    Had to reopen to fix compilation errors that stopped the other testing.
# 	    Removed the token manager syscall routine from user-space compilation.
# 	    This version of this delta contains support for the token manager's
# 	    periodic activites thread (not yet activated) and for the afs_syscall
# 	    that will query and set various parameters that control the token manager.
# 	    Put ifdefs around prototype for routine that's only needed if testing without
# 	    other tsr deltas.
# 	    Check-point so that I can import another delta.
# 	    Support for token manager periodic activities thread, from within
# 	    the core kernel extension.
# 	    Build object file containing osi_NewProc() in user-space
# 	    and include it in the appropriate library.
# 	    Allow client routines to return (exit the thread) without
# 	    getting error messages.  Also changed the type of the thread base
# 	    routine to void.
# 	    Changed interface to osi_NewProc to support parameterization of
# 	    grabbing of kernel lock (on RIOS....blocking & restoration of kernel
# 	    preemption, in general).
# 	    Correct typo.
# 	    Set up for debugging.
# 	    Sometimes, the token manager helper thread would get an illegal trap
# 	    instruction on a rios machine (which Carl tells me is what assert
# 	    failures become when the code is shipped) on trying to pause (via
# 	    osi_Pause/osi_Wait). The problem is that the timer blocks in the kernel
# 	    had not been initialized by the time the token manager thread tried to run.
# 	    This version of this delta fixes the problem by making sure the timer
# 	    blocks are initialized in time.
# 	    Removed debugging control from testing of gc/async grant thread.
# 	    Debugged problems with tkm not identifying potential candidates if the
# 	    testing goes too fast (the reference times get too close together),
# 	    or tkm tries to gc a fid on which it already holds the lock.
# 	    Changed name of TakeBackTokens routine to reflect its new placement.
# 	Transarc delta: jdp-ot3317-osi-move-lock.o-to-lwpdummy.a 1.1
# 	  Selected comments:
# 	    This delta moves lock.o from libosi.a to liblwpdummy.a to help prevent
# 	    inadvertent use of it in user programs.
# 	    Move lock.o from libosi.a to liblwpdummy.a.
# 	[1992/08/30  03:14:50  jaffe]
# 
# Revision 1.1.3.10  1992/05/20  19:58:19  mason
# 	Transarc delta: bwl-build-lwpdummy 1.2
# 	  Files modified:
# 	    osi: Makefile, lwpdummy.c
# 	  Selected comments:
# 	    Separate lwpdummy library from osi library, so Episode test_anode and
# 	    test_vnodeops don't get the former.
# 	    Separate lwpdummy library from osi library.
# 	    More changes required:  more dummy definitions restored to lwpdummy.c.
# 	[1992/05/20  11:45:15  mason]
# 
# Revision 1.1.3.9  1992/05/13  22:02:24  jdp
# 	Transarc delta: jdp-ot3317-osi-move-lock.o-to-lwpdummy.a 1.1
# 	  Files modified:
# 	    osi: Makefile
# 	  Selected comments:
# 	    This delta moves lock.o from libosi.a to liblwpdummy.a to help prevent
# 	    inadvertent use of it in user programs.
# 	    Move lock.o from libosi.a to liblwpdummy.a.
# 	[1992/05/13  18:04:40  jdp]
# 
# Revision 1.1.3.8  1992/05/06  19:28:42  jdp
# 	Transarc delta: bwl-build-lwpdummy 1.2
# 	  Files modified:
# 	    osi: Makefile, lwpdummy.c
# 	  Selected comments:
# 	    Separate lwpdummy library from osi library, so Episode test_anode and
# 	    test_vnodeops don't get the former.
# 	    Separate lwpdummy library from osi library.
# 	    More changes required:  more dummy definitions restored to lwpdummy.c.
# 	[1992/05/06  18:49:29  jdp]
# 
# Revision 1.1.3.7  1992/05/05  21:46:31  mason
# 	delta jdp-ot2556-osi-put-tracing-back 1.4
# 	delta jdp-ot2611-osi-add-copyrights 1.2
# 	[1992/05/05  21:34:44  mason]
# 
# Revision 1.1.3.6  1992/04/21  13:51:55  mason
# 	Transarc delta: cburnett-ot2477-fix_bld_bugs_w_aix31_vm 1.1
# 	  Files modified:
# 	    cm: cm_scache.c, cm_scache.h; libafs/RIOS: dfscore.exp
# 	    osi: Makefile, osi_vmm.h; xvnode: xvfs_vnode.c
# 	    xvnode/RIOS: xvfs_aix2vfs.c, xvfs_aixglue.c
# 	  Selected comments:
# 	    [ This change fixes compile bugs genereated when AFS_AIX31_VM is
# 	      defined in config/RIOS/param.h.  It allows the AIX VMM integration
# 	      pieces of Eposode to be enabled.  It should be noted however the
# 	      the VMM integration of the DFS cache manager with AIX is not complete
# 	      or functional.  It some later time a submission will be made to
# 	      remove the AFS_AIX31_VM definition and move it into the Episode
# 	      specific code so that Episode can be compiled with the VMM turned
# 	      on and the cache manager without it.]
# 	    [ add export of osi_vmm.h]
# 	Transarc delta: cburnett-ot2479-add_osi_device.h 1.1
# 	  Files modified:
# 	    osi: Makefile, osi_device.h
# 	  Selected comments:
# 	    [ add osi_device to hold porting interfaces relative to devices and
# 	      device structures]
# 	    [ added export of osi_device.h]
# 	Transarc delta: jdp-ot2534-osi-remove-osi_dkload.h 1.2
# 	  Files modified:
# 	    osi: Makefile, osi_misc.c
# 	  Selected comments:
# 	    Remove the vestigial file "osi/osi_dkload.h".
# 	    Removed "osi_dkload.h" from list of includes.
# 	    Forgot to remove #include line in "osi_misc.c".  It was available in
# 	    the backing tree, so the test compile still succeeded.
# 	Transarc delta: tu-1143-error-mapping 1.8
# 	  Files modified:
# 	    cm: cm.h, cm_cell.c, cm_conn.c, cm_daemons.c, cm_dcache.c
# 	    cm: cm_dnamehash.c, cm_init.c, cm_pioctl.c, cm_rrequest.c
# 	    cm: cm_scache.c, cm_server.c, cm_subr.c, cm_tokens.c
# 	    cm: cm_vnodeops.c, cm_volume.c; fsprobe: fsprobe.c
# 	    libafs/RIOS: dfscore.exp; osi: Makefile, osi_dfserrors.h
# 	    osi.klib/OSF1: Makefile; osi.klib/RIOS: Makefile
# 	    osi/OSF1: Makefile, osi_dfs2osferr.c, osi_osf2dfserr.c
# 	    osi/RIOS: Makefile, osi_aix2dfserr.c, osi_dfs2aixerr.c
# 	    px: px_init.c, px_intops.c; rep: rep_main.c
# 	    userInt/fts: volc_tokens.c
# 	  Selected comments:
# 	    An enhancement to allow the DFS server to map returned kernel errors to the
# 	    canonical ones and to allow the CM to map the canonical ones to the
# 	    appropriate local kernel errors.
# 	    Please refer to the design memo, Error Conversion Protocol, for more detail.
# 	    Added osi_dfserrros.h to the Makefile
# 	    The same as above.
# 	    Made osi_dfserrors.h and osi_dfs2aixerr.c osi_aix2dfserr.c osi_osf2dfserr.c and
# 	    osi_dfs2osferr.c avaliable in the user space. So, whoever references AFS_xxx
# 	    APIs should use the error conversion protocol.
# 	    fixed some typo errors and others
# 	    Changes did not compile on the PMAX.
# 	    DFS servers now maps errors to the canonical ones before returning to theirs
# 	    clients. Therefore, those clients who use AFS_xxx rpc calls must do the
# 	    error decoding work to get the appropriate error back.
# 	    CFE adds osi_initDecodeTable() and osi_errDecode() to repserver and fts.
# 	[1992/04/20  22:43:41  mason]
# 
# Revision 1.1.3.5  1992/04/14  04:09:52  mason
# 	Transarc delta: tu-1143-error-mapping 1.6
# 	  Files modified:
# 	    cm: cm.h, cm_cell.c, cm_conn.c, cm_daemons.c, cm_dcache.c
# 	    cm: cm_dnamehash.c, cm_init.c, cm_pioctl.c, cm_rrequest.c
# 	    cm: cm_scache.c, cm_server.c, cm_subr.c, cm_tokens.c
# 	    cm: cm_vnodeops.c, cm_volume.c; libafs/RIOS: dfscore.exp
# 	    osi: Makefile, osi_dfserrors.h; osi.klib/OSF1: Makefile
# 	    osi.klib/RIOS: Makefile
# 	    osi/OSF1: Makefile, osi_dfs2osferr.c, osi_osf2dfserr.c
# 	    osi/RIOS: Makefile, osi_aix2dfserr.c, osi_dfs2aixerr.c
# 	    px: px_init.c, px_intops.c
# 	  Selected comments:
# 	    An enhancement to allow the DFS server to map returned kernel errors to the
# 	    canonical ones and to allow the CM to map the canonical ones to the
# 	    appropriate local kernel errors.
# 	    Please refer to the design memo, Error Conversion Protocol, for more detail.
# 	    Added osi_dfserrros.h to the Makefile
# 	    The same as above.
# 	    Made osi_dfserrors.h and osi_dfs2aixerr.c osi_aix2dfserr.c osi_osf2dfserr.c and
# 	    osi_dfs2osferr.c avaliable in the user space. So, whoever references AFS_xxx
# 	    APIs should use the error conversion protocol.
# 	    fixed some typo errors and others
# 	    Changes did not compile on the PMAX.
# 	[1992/04/13  17:01:47  mason]
# 
# Revision 1.1.3.4  1992/01/31  14:46:15  delgado
# 	Add modifications to build libdcedfs.a
# 	[1992/01/31  14:45:53  delgado]
# 
# Revision 1.1.3.3  1992/01/24  03:46:39  devsrc
# 	Merging dfs6.3
# 	[1992/01/24  00:16:41  devsrc]
# 
# Revision 1.1.3.2  1992/01/23  20:19:34  zeliff
# 	Moving files onto the branch for dfs6.3 merge
# 	[1992/01/23  18:36:20  zeliff]
# 	Revision 1.1.1.3  1992/01/23  22:19:04  devsrc
# 	Fixed logs
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.5 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/osi
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

CFILES=		afsl_trace.c \
		lock.c \
		osi_dfs2oserr.c \
		osi_fio.c \
		osi_linkage.c \
		osi_lock_mach.c \
		osi_lock_single.c \
		osi_misc.c \
		osi_net.c \
		osi_net_irix.c \
		osi_os2dfserr.c \
		osi_pag.c \
		osi_port_os.c \
		osi_printf.c \
		osi_trace_init.c \
		osi_ufs.c \
		osi_ufs_mach.c \
		uiomove.c

HFILES=		lock.h \
		osi.h \
		osi_alloc.h \
		osi_buf.h \
		osi_buf_mach.h \
		osi_calltrace.h \
		osi_cred.h \
		osi_cred_mach.h \
		osi_device.h \
		osi_device_mach.h \
		osi_dfserrors.h \
		osi_intr.h \
		osi_intr_mach.h \
		osi_kvnode.h \
		osi_kvnode_mach.h \
		osi_lock_mach.h \
		osi_lock_pthread.h \
		osi_lock_single.h \
		osi_mount.h \
		osi_net.h \
		osi_net_mach.h \
		osi_param.h \
		osi_port.h \
		osi_port_mach.h \
		osi_sysconfig.h \
		osi_ufs.h \
		osi_ufs_mach.h \
		osi_uio.h \
		osi_uio_mach.h \
		osi_user.h \
		osi_user_mach.h \
		osi_sysconfig_mach.h \
		osi_vmm.h \
		osi_vmm_mach.h \
		osi_wait.h \
		queue.h \
		sysincludes.h

OBJ_NAME=	libkosi.a
OBJS=		$(CFILES:.c=.o)
OBJ_FILES=	$(OBJS) \

$(OBJ_NAME):$(OBJ_NAME)($(OBJ_FILES)) MAKELIB

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(OBJ_NAME)

$(KCOMMONPREF)install:$(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

