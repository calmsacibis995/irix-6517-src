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
# Revision 65.2  1999/02/04 22:37:18  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.1  1997/10/20 19:15:44  jdoak
# *** empty log message ***
#
# Revision 1.1.6.1  1996/10/03  14:41:29  arvind
# 	clean up for build on 10.x
# 	[1996/09/17  23:29 UTC  sommerfeld  /main/sommerfeld_pk_kdc_1/1]
#
# Revision 1.1.4.2  1996/03/09  23:25:53  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:41:33  marty]
# 
# Revision 1.1.4.1  1995/12/07  22:33:44  root
# 	Submit OSF/DCE 1.2.1
# 
# 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/08  20:52 UTC  dat
# 	Expand lex_yy.c dependency from /usr/lib/lex/*form to /usr/lib/lex/ncform and /usr/lib/lex/nrform
# 
# 	HP revision /main/HPDCE02/2  1995/01/30  22:39 UTC  sommerfeld
# 	hpdce_version.h is gone.
# 	[1995/01/27  04:19 UTC  sommerfeld  /main/HPDCE02/sommerfeld_mothmk_2/1]
# 
# 	HP revision /main/HPDCE02/1  1994/08/03  16:33 UTC  tatsu_s
# 	Merged mothra up to dce 1.1.
# 
# 	HP revision /main/HPDCE01/3  1994/02/01  22:30  tatsu_s
# 	Merged tatsu_s_ccase_fix_b1 to HPDCE01.
# 
# 	HP revision /main/HPDCE01/tatsu_s_ccase_fix_b1/2  1994/02/01  21:20  tatsu_s
# 	Disabled the variable evaluations on ':' command.
# 
# 	HP revision /main/HPDCE01/tatsu_s_ccase_fix_b1/1  1994/02/01  16:49  tatsu_s
# 	Made EVs not being echoed.
# 
# 	HP revision /main/HPDCE01/2  1994/01/24  22:29  tatsu_s
# 	Submitted tatsu_s_ccase_fix_b0.
# 	Added the capture of the critical environments in the translation rules.
# 
# 	HP revision /main/HPDCE01/1  1994/01/21  22:48  lmm
# 	merge kk with hpdce01
# 	[1995/12/07  21:17:13  root]
# 
# Revision 1.1.8.3  1993/09/20  18:00:21  tatsu_s
# 	KK merged upto DCE1_0_3b03.
# 
# 	Added INCDIRS for hpdce_version.h
# 	[1993/05/28  19:09:27  tatsu_s]
# 
# Revision 1.1.9.3  1993/09/15  15:23:32  root
# 	    Loading drop DCE1_0_3b03
# 	Revision 1.1.8.2  1993/07/08  09:08:57  root
# 	Initial King Kong branch
# 	[1993/07/08  09:08:30  root]
# 
# Revision 1.1.2.3  1993/01/03  22:14:08  bbelch
# 	Embedding copyright notice
# 	[1993/01/03  14:42:35  bbelch]
# 
# Revision 1.1.2.2  1992/12/23  19:13:44  zeliff
# 	Embedding copyright notice
# 	[1992/12/23  01:09:50  zeliff]
# 
# Revision 1.1  1992/01/19  03:00:27  devrcs
# 	Initial revision
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.2 $
DEPTH?=../../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/rpc/idl/nidl_to_idl
.PATH:$(KPATH)

HFILES=		idl_base.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	$(INSTALL) -m 444 -F /usr/include/dce $(HFILES)

