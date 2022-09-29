#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
# src directory for the full copyright text.
#
# (c) Copyright 1991, 1992 
#     Siemens-Nixdorf Information Systems, Burlington, MA, USA
#     All Rights Reserved
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.6  1999/05/21 18:53:49  lauriec
# Moved building into rpc/kruntime, so its deleted from here.
#
# Revision 65.5  1999/02/04 22:37:19  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.4  1998/01/07 15:00:34  gwehrman
# Removed references to OBJS_COMPAT.  It doesn't appear that this variable
# is ever set, and its presence breaks some of the build targets such as
# clobber_all.
#
# Revision 65.3  1997/12/12  14:30:14  gwehrman
# Export library using cp instead of a symbolic link.
#
# Revision 65.2  1997/10/20  19:16:38  jdoak
# Initial IRIX 6.4 code merge
#
# Revision 1.2.24.2  1996/10/15  20:46:47  arvind
# 	Change for international build
# 	[1996/10/07  19:27 UTC  arvind  /main/arvind_work/2]
#
# 	Update OSF copyright year
# 	[1996/03/09  22:42:02  marty]
#
# 	ENDGAME Submission
# 	[1996/02/22  16:10:54  root]
#
# 	Submit OSF/DCE 1.2.1
# 	HP 	[1995/11/16  21:29 UTC  jrr  /main/HPDCE02/gaz_dce_instr/jrr_1.2_mothra/1]
#
# 	Remove some Mothra specific stuff.
# 	HP 	[1995/07/20  18:31 UTC  gaz  /main/HPDCE02/gaz_dce_instr/2]
#
# 	Remove define of 'USE_GETTIMEOFDAY' to header files
# 	HP 	[1995/07/15  20:17 UTC  gaz  /main/HPDCE02/gaz_dce_instr/1]
#
# 	cause server stubs to be compiled with instrumentation
# 	HP 	[1995/01/16  19:14 UTC  markar  /main/HPDCE02/5]
#
# 	Merging Local RPC mods
# 	HP 	[1995/01/05  16:03 UTC  markar  /main/HPDCE02/markar_local_rpc/1]
#
# 	Implementing Local RPC bypass mechanism
# 	HP 	[1994/12/23  00:25 UTC  kline_s  /main/HPDCE02/4]
#
# 	Enable RPC I18N functionality
# 	HP 	[1994/12/22  21:38 UTC  kline_s  /main/HPDCE02/kline_s_mothra_latest/1]
#
# 	Add dce_langinfo() to object list.
# 	HP 	[1994/09/21  15:11 UTC  tatsu_s  /main/HPDCE02/3]
#
# 	Fixed DES_HIDDEN build.
# 	HP 	[1994/09/21  14:50 UTC  tatsu_s  /main/HPDCE02/2]
#
# 	Merged mothra up to dce1_1_b18.
# 	[1995/12/07  23:57:00  root]
#
# 	Merged mothra up to dce 1.1.
# 	[1993/10/05  19:59:01  tatsu_s  1.1.12.5]
#
# 	Loading drop DCE1_0_3b931005
# 	[1993/09/20  18:00:47  tatsu_s  1.1.12.4]
#
# Revision 1.2.24.1  1996/05/10  13:08:46  arvind
# 	Drop 1 of DCE 1.2.2 to OSF
# 
# 	HP revision /main/DCE_1.2/2  1996/03/14  19:10 UTC  psn
# 	Merge OSF endgame fixes to HP.
# 
# 	HP revision /main/DCE_1.2/psn_1.2_endgame/1  1996/02/28  21:53 UTC  psn
# 	Fix OT13343 as done during OSF 1.2.1 Endgame.
# 
# 	HP revision /main/DCE_1.2/1  1996/01/03  18:57 UTC  psn
# 	Remove some Mothra specific stuff.
# 	[1995/11/16  21:29 UTC  jrr  /main/HPDCE02/gaz_dce_instr/jrr_1.2_mothra/1]
# 
# 	Remove define of 'USE_GETTIMEOFDAY' to header files
# 	[1995/07/20  18:31 UTC  gaz  /main/HPDCE02/gaz_dce_instr/2]
# 
# 	cause server stubs to be compiled with instrumentation
# 	[1995/07/15  20:17 UTC  gaz  /main/HPDCE02/gaz_dce_instr/1]
# 
# 	Merging Local RPC mods
# 	[1995/01/16  19:14 UTC  markar  /main/HPDCE02/5]
# 
# 	Implementing Local RPC bypass mechanism
# 	[1995/01/05  16:03 UTC  markar  /main/HPDCE02/markar_local_rpc/1]
# 
# 	Enable RPC I18N functionality
# 	[1994/12/23  00:25 UTC  kline_s  /main/HPDCE02/4]
# 
# 	Add dce_langinfo() to object list.
# 	[1994/12/22  21:38 UTC  kline_s  /main/HPDCE02/kline_s_mothra_latest/1]
# 
# 	Fixed DES_HIDDEN build.
# 	[1994/09/21  15:11 UTC  tatsu_s  /main/HPDCE02/3]
# 
# 	Merged mothra up to dce1_1_b18.
# 	[1994/09/21  14:50 UTC  tatsu_s  /main/HPDCE02/2]
# 
# 	CR # 11497 - Remove '-v' option from IDLFLAGS defineittion.
# 	[1994/09/06  21:39:31  marty]
# 	#
# 
# 	Merged mothra up to dce 1.1.
# 	[1994/08/03  16:34 UTC  tatsu_s  /main/HPDCE02/1]
# 
# 	Merged up to DCE1_0_3b931005.
# 	#
# 	Added the build rule for libnck.sl
# 	[1993/07/28  15:51:47  tatsu_s]
# 	#
# 	Add changes for DES-hidden build.
# 	[1992/12/07  14:46:11  grober_j]
# 	#
# 
# 	Loading drop DCE1_0_3b931005
# 	[1993/10/05  17:59:01  tatsu_s  1.1.13.5]
# 
# 	KK merged upto DCE1_0_3b03.
# 	[1993/09/18  05:37:25  tatsu_s]
# 
# Revision 1.2.18.10  1994/09/07  15:48:34  marty
# 	CR # 11497 - Remove '-v' option from IDLFLAGS defineittion.
# 	[1994/09/06  21:39:31  marty]
# 
# Revision 1.2.18.9  1994/07/14  21:28:30  tom
# 	Remove NCS compat stuff.
# 	[1994/07/14  21:09:10  tom]
# 
# Revision 1.2.18.8  1994/06/29  20:55:23  ohara
# 	Merged with changes from 1.2.18.7
# 	[1994/06/29  20:55:12  ohara]
# 
# 	cs_s_stub.o not supported on SVR4 yet
# 	[1994/06/27  19:54:13  ohara]
# 
# Revision 1.2.18.7  1994/06/23  17:40:10  tom
# 	Add uuidsys.o since it now is mostly machine independant.
# 	[1994/06/22  20:40:33  tom]
# 
# Revision 1.2.18.6  1994/05/20  22:03:36  sommerfeld
# 	Merged with changes from 1.2.18.5
# 	[1994/05/20  22:02:50  sommerfeld]
# 
# 	Merge in Will's changes
# 	[1994/05/20  21:36:44  sommerfeld]
# 
# 	add dependancy on codesets.h for a few files.
# 	[1994/05/17  19:09:47  sommerfeld]
# 
# Revision 1.2.18.5  1994/05/19  21:14:16  hopkins
# 	Merge with DCE1_1.
# 	[1994/05/04  19:34:51  hopkins]
# 
# 	Serviceability:
# 	  Add OBJS_SVC to OFILES, remove rpcdbg.o
# 	  from OBJS_COMMON, add SVC_DEFS and rule
# 	  to build .c and .h files from rpc.sams.
# 	[1994/05/03  20:16:34  hopkins]
# 
# Revision 1.2.18.4  1994/02/09  21:06:50  mori_m
# 	CR 9912:  Moved machine dependent object (cs_s_conv.o) to rpc.mk
# 	[1994/02/09  21:06:38  mori_m]
# 
# Revision 1.2.18.3  1994/02/08  21:29:03  mori_m
# 	CR 9701:  Initial submission for RPC runtime I18N support
# 	[1994/02/08  21:28:48  mori_m]
# 
# Revision 1.2.18.2  1994/01/04  17:50:07  ohara
# 	fix include of machdep.mk for new make
# 	[1994/01/04  17:34:30  ohara]
# 
# Revision 1.2.18.1  1993/12/14  15:49:48  rsalz
# 	Remove dce_error.o; it is now in SVC library.
# 	[1993/12/14  15:49:40  rsalz]
# 
# Revision 1.2.13.1  1993/09/15  15:39:42  damon
# 	Synched with ODE 2.1 based build
# 	[1993/09/15  15:31:57  damon]
# 
# Revision 1.2.13.2  1993/09/14  16:47:37  tatsu_s
# 	Bug 8103 - Added comfork_cma.o into OBJS_COMMON.
# 	[1993/09/13  17:01:42  tatsu_s]
# 
# Revision 1.2.6.5  1993/01/03  22:57:00  bbelch
# 	Embedding copyright notice
# 	[1993/01/03  19:56:51  bbelch]
# 
# Revision 1.2.6.4  1992/12/23  20:10:01  zeliff
# 	Embedding copyright notice
# 	[1992/12/23  15:27:54  zeliff]
# 
# Revision 1.2.6.3  1992/10/02  19:37:02  markar
# 	  OT CR 2786 fix: Relocated information from rpc.mk to here
# 	[1992/09/22  18:48:47  markar]
# 
# Revision 1.2.6.2  1992/09/29  21:13:53  devsrc
# 	SNI/SVR4 merge.  OT 5373
# 	[1992/09/11  15:36:33  weisman]
# 
# Revision 1.2.2.7  1992/05/27  13:25:44  rec
# 	Removed -g option from the compile line.  Create a libdce that is too
# 	large for the current version of the OSF/1 langauge tools (linker).
# 	[1992/05/27  13:25:00  rec]
# 
# Revision 1.2.2.6  1992/05/22  18:56:54  wei_hu
# 	AUTH CN Code Drop (RPC6)
# 	[1992/05/22  18:41:40  wei_hu]
# 
# Revision 1.2.2.5  1992/05/11  17:51:19  mhickey
# 	Removed obsolete define of NEED_PROJECT_MAKE_DEFS
# 	and include of osf.dce.mk.  Fixes defect 2031.
# 	[1992/04/22  15:21:51  mhickey]
# 
# Revision 1.2.2.4  1992/05/06  20:10:46  weisman
# 	Correct typo for NOPping out CN/auth.
# 
# 	<AUTH_DEFS	?= -DAUTH_KRB -DUATH_KRB_DG
# 	>AUTH_DEFS	?= -DAUTH_KRB -DAUTH_KRB_DG
# 	[1992/05/06  20:10:24  weisman]
# 
# Revision 1.2.2.3  1992/05/01  18:37:04  rsalz
# 	"Changed as part of rpc6 drop."
# 	[1992/05/01  18:25:24  rsalz]
# 
# 	 10-mar-1992	pf	DCE V1.0.1 merge
# 	[1992/05/01  18:05:47  rsalz]
# 
# Revision 1.2  1992/01/19  22:13:39  devrcs
# 	Dropping DCE1.0 OSF1_misc port archive
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.6 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/rpc/runtime
.PATH:$(KPATH)

TARGETS=

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install:$(KCOMMONPREF)default

headers!

