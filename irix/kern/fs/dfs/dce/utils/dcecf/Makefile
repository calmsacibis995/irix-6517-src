#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.3  1999/02/03 21:24:52  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.2  1997/10/20 19:15:25  jdoak
# Initial IRIX 6.4 code merge
#
# Revision 1.1.15.2  1996/03/09  23:11:03  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:28:34  marty]
#
# Revision 1.1.15.1  1995/12/08  21:36:44  root
# 	Submit OSF/DCE 1.2.1
# 
# 	HP revision /main/HPDCE02/5  1994/11/23  18:01 UTC  sommerfeld
# 	flush busted merge.
# 	[1994/11/22  21:22 UTC  sommerfeld  /main/HPDCE02/sommerfeld_mothmk_1/1]
# 
# 	HP revision /main/HPDCE02/4  1994/10/03  16:24 UTC  root
# 	Merge from Hpdce02_03
# 	[1995/12/08  18:08:33  root]
# 
# Revision 1.3  1996/09/27  15:45:26  bhim
# Improved SGIMIPS install condition checks.
#
# Revision 1.2  1996/09/11  16:54:27  bhim
# Modified for new build procedure.
#
# Revision 1.1  1994/09/28  20:43:16  dcebuild
# Original File from OSF
#
# Revision 1.1.13.8  1994/09/28  20:43:15  bowe
# 	Add dcecfgmsg.o [CR 12366]
# 	[1994/09/28  20:42:28  bowe]
# 
# 	HP revision /main/HPDCE02/3  1994/09/01  11:42 UTC  mgm
# 	fix so that dce_cf.h gets installed during install pass
# 
# Revision 1.1.13.7  1994/07/27  17:28:14  rsalz
# 	Add STRICT_COMPILER_WARNINGS and cleanup resultant output.
# 	[1994/07/27  17:27:42  rsalz]
# 
# Revision 1.1.13.6  1994/07/13  15:04:06  marty
# 	CR # 10017 - Set permissions on header files to 444 during install pass
# 	[1994/07/12  15:53:20  marty]
# 
# Revision 1.1.13.5  1994/06/09  16:05:38  devsrc
# 	cr10892 - fix copyright
# 	[1994/06/09  15:50:16  devsrc]
# 
# Revision 1.1.13.4  1994/01/18  23:17:28  bowe
# 	Ensure ILIST and IDIR set to the right values.
# 	[1994/01/18  23:15:55  bowe]
# 
# Revision 1.1.13.3  1994/01/10  20:04:22  rsalz
# 	Remove dcecfg.{cat,msg} (OT CR 9692).
# 	Write dce_cf_get_csrgy_filename (OT CR 9696).
# 	[1994/01/10  19:55:24  rsalz]
# 
# Revision 1.1.13.2  1994/01/03  19:35:03  rsalz
# 	Add LIBRARIES to ILIST (OT CR 9663).
# 	Remove some dependencies.
# 	[1994/01/03  19:34:52  rsalz]
# 
# Revision 1.1.13.1  1993/12/29  19:22:25  rsalz
# 	Move dce_cf_XXX from config to dce/utils/dcecf (OT CR 9663).
# 	Rewrite to fix various bugs (OT CR 9665).
# 	[1993/12/29  16:17:53  rsalz]
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.3 $
DEPTH?=../../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/dce/utils/dcecf
.PATH:$(KPATH)

KCINCS=		-I$(ROOT)/usr/include \
		-I$(ROOT)/usr/include/dcedfs

OBJ_NAME=	
OBJ_CSRCS=	
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		dce_cf_const.h dce_cf.h dcecfgmsg.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dce $(HFILES)

