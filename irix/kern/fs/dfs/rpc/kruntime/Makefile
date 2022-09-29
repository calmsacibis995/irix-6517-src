#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
# src directory for the full copyright text.
#
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.14  1999/05/21 18:53:49  lauriec
# Changes for building rpc/runtime in rpc/kruntime
#
# Revision 65.13  1999/03/23 22:57:00  gwehrman
# Added special rule for rpc_icrash.c so that it is compiled with symbols.
# Fix for 680479. 
#
# Revision 65.12  1999/02/05 15:44:20  mek
# Add hash symbols to ensure comments are not taken as directives.
#
# Revision 65.11  1999/02/04 23:15:10  mek
# Makefile tuning.
#
# Revision 65.10  1999/02/04 22:37:19  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.9  1999/01/20 14:24:49  gwehrman
# Added path to kdes directory as an include flag.  This is needed to find the
# new kdes_inline.h header file.
#
# Revision 65.8  1998/05/06 18:56:40  lmc
# Added idbg_helper.c to the makefile for krpc helper structures.
#
# Revision 65.7  1998/03/24  16:21:40  lmc
# Fixed missing comment characters lost when the ident lines were ifdef'd
# for the kernel.  Also now calls rpc_icrash_init() for
# initializing symbols, and the makefile changed to build rpc_icrash.c
# instead of dfs_icrash.c.
#
# Revision 65.5  1998/02/27  17:32:06  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.4  1998/02/27 16:16:05  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.3  1998/02/26 23:37:17  lmc
# Removed setting of TARGET_FLAGS because it is set in SGIMIPS/machdep.mk.
# Changed comsoc_sys.o to comsoc_sys1.o and comsoc_sys2.o for the split of
# the select specific code.  Removed KERNELCFLAGS because it isn't necessary.
#
# Revision 65.2  1997/10/20  19:16:24  jdoak
# Initial IRIX 6.4 code merge
#
# Revision 64.3  1997/03/31  15:37:04  lord
# Make debug compile flags globally switchable
#
# Revision 64.1  1997/02/14  19:45:22  dce
# *** empty log message ***
#
# Revision 1.6  1996/09/11  16:58:27  bhim
# Modified for new build procedure.
#
# Revision 1.5  1996/08/27  15:49:17  bhim
# Modified for new build procedure.
#
# Revision 1.4  1996/05/25  16:44:02  brat
# Modified stuff to build kernel-related stuff to be 32-bit or 64-bit and the
# rest to be n32. This way we have one build instead of multiple builds.
#
# Revision 1.3  1996/04/19  23:16:28  brat
# Changes to make it run for both 64-bit and 32-bit machines
#
# Revision 1.2  1996/04/15  17:23:03  vrk
# Changes for SGIMIPS.
#
# Revision 1.1  1994/09/15  13:36:19  dcebuild
# Original File from OSF
#
# Revision 1.2.14.5  1994/09/15  13:36:19  rsarbo
# 	build svc_krpc.o and dcerpcmsg.o into KRPC to restore
# 	minimal printf functionality with new S12Y interfaces
# 	[1994/09/15  13:20:42  rsarbo]
#
# Revision 1.2.14.4  1994/09/07  15:48:33  marty
# 	CR # 11497 - Remove '-v' option from IDLFLAGS defineittion.
# 	[1994/09/06  21:39:30  marty]
# 
# Revision 1.2.14.3  1994/05/19  21:14:12  hopkins
# 	Serviceability work
# 	[1994/05/19  02:17:42  hopkins]
# 
# 	Serviceability:
# 	  add SVC_DEFS to CFLAGS.
# 	[1994/05/18  21:33:15  hopkins]
# 
# Revision 1.2.14.2  1994/01/21  22:31:34  cbrooks
# 	Code Cleanup
# 	[1994/01/21  22:29:14  cbrooks]
# 
# Revision 1.2.14.1  1993/12/07  17:54:40  mario
# 	Changed for ode2.3 - < and > becomes this except for RULES_MK
# 	[1993/12/07  17:25:28  mario]
# 
# Revision 1.2.11.1  1993/09/01  14:42:10  delgado
# 	Dont put -UKNCK -DKNCK on the same compile line - it does not work
# 	the way it used to on OSF1 (now works the way it should).
# 	[1993/09/01  14:41:58  delgado]
# 
# Revision 1.2.8.2  1993/06/10  19:26:10  sommerfeld
# 	Initial HPUX RP version.  Don't add OTHERS to EXPLIB_TARGETS
# 	by default.  This may not be correct in the long term.
# 	[1993/06/03  22:35:30  kissel]
# 
# Revision 1.2.4.4  1993/01/03  22:35:14  bbelch
# 	Embedding copyright notice
# 	[1993/01/03  19:50:48  bbelch]
# 
# Revision 1.2.4.3  1992/12/23  19:37:10  zeliff
# 	Embedding copyright notice
# 	[1992/12/23  14:48:57  zeliff]
# 
# Revision 1.2.4.2  1992/12/03  22:07:19  delgado
# 	Export kprc_helper_mach.h
# 	[1992/12/03  22:06:38  delgado]
# 
# Revision 1.2.2.5  1992/06/26  06:28:12  jim
# 	Added ../runtime/${TARGET_MACHINE} to vpath list.
# 	[1992/06/23  16:01:08  jim]
# 
# Revision 1.2.2.4  1992/05/04  21:00:42  rsalz
# 	Set OBJECT_FORMAT for OSF/1 pmax kernels
# 	[1992/05/04  20:59:12  rsalz]
# 
# Revision 1.2.2.3  1992/05/04  15:23:57  rsalz
# 	Add KERNEL_CC= line so that PMAX/machdep.mk sets right object format.
# 	[1992/05/04  15:23:34  rsalz]
# 
# Revision 1.2.2.2  1992/05/01  17:36:45  rsalz
# 	"Changed pas part of rpc6 drop."
# 	[1992/05/01  17:34:26  rsalz]
# 
# Revision 1.2  1992/01/19  22:13:32  devrcs
# 	Dropping DCE1.0 OSF1_misc port archive
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.14 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/rpc/kruntime
.PATH:$(KPATH) $(KPATH)/../runtime $(KPATH)/../sys_idl

SUBDIRS=	stubs

KCINCS=		-I$(KPATH) \
		-I$(KPATH)/../kdes \
		-I$(KPATH)/../runtime \
		-I$(ROOT)/usr/include \
		-I$(ROOT)/usr/include/dcedfs

KCDEFS +=	-DNCK -DINET -DPROT_NCADG -DAUTH_KRB -DAUTH_KRB_DG \
		-DNAF_IP -DDCE_RPC_SVC

CFILES=		comsoc_sys1.c \
		comsoc_sys2.c \
		ipnaf_sys.c \
		pthread_sys.c \
		uuidsys.c \
		route_cache.c \
		krpc_helper_mach.c \
		idbg_lis_state.c \
		idbg_call.c \
		idbg_cct.c \
		idbg_sct.c \
		idbg_rpc_misc.c \
		idbg_helper.c \
		rpc_icrash.c \
		md5_krpc.c \
		sec_pkl_krpc.c \
		pkl_krpc.c \
		pac_util_krpc.c \
		comnlsn_krpc.c \
		dce_error_krpc.c \
		subr_krpc.c \
		idl_ss_krpc.c \
		sec_authn_krpc.c \
		krpc_helper.c \
		svc_krpc.c

# files from ../runtime directory
CFILES +=       comauth.c \
                combind.c \
                comcall.c \
                comcthd.c \
                comif.c \
                cominit.c \
                cominit_ux.c \
                comnaf.c \
                comnet.c \
                comobj.c \
                comp.c \
                comutil.c \
                rpcuuid.c \
                rpctimer.c \
                rpcclock.c \
                rpcdbg.c \
                rpcmutex.c \
                rpclist.c \
                rpcmem.c \
                conv.c \
                dg.c \
                dgcall.c \
                dgccall.c \
                dgccallt.c \
                dgcct.c \
                dgclive.c \
                dgclsn.c \
                dgexec.c \
                dgfwd.c \
                dgglob.c \
                dghnd.c \
                dginit.c \
                dglossy.c \
                dglsn.c \
                dgpkt.c \
                dgrq.c \
                dgscall.c \
                dgsct.c \
                dgslive.c \
                dgslsn.c \
                dgsoc.c \
                dgutl.c \
                dgxq.c \
                ipnaf.c \
                mgmt.c \
                ndrglob.c \
                krbclt.c \
                krbcom.c \
                krbcrc.c \
                krbdgclt.c \
                krbdgcom.c \
                krbdgsrv.c \
                krbreq.c

# files from ../sys_idl directory
CFILES +=	dcerpcmsg.c

SFILES=		pthread_asm.s

OBJS=		$(CFILES:.c=.o) $(SFILES:.s=.o)
TARGETS=	$(OBJS)
HFILES=		exc_handling.h \
		exc_handling_ids_krpc.h \
		idl_ss_krpc.h \
		krpc_helper.h \
		krpc_helper_mach.h \
		pthread.h \
		pthread_exc.h

OBJ_NAME = libknck.a
OBJ_FILES =	$(OBJS)

$(OBJ_NAME):$(OBJ_NAME)($(OBJ_FILES)) MAKELIB

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(OBJ_NAME)

$(KCOMMONPREF)install:$(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dce/ker $(HFILES)

rpc_icrash.o: rpc_icrash.c
	$(CCF) -g3 -c $(JALR) $(KPATH)/rpc_icrash.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)
