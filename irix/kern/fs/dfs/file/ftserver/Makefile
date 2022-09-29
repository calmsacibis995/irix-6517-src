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
# Revision 65.4  1999/02/04 19:19:40  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.3  1998/03/04 21:58:31  gwehrman
# Changed ftserver_LIBS definition to fix unresolved symbols.
#
# Revision 65.2  1997/12/16 17:05:34  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:20:08  jdoak
# *** empty log message ***
#
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.4 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/ftserver
.PATH:$(KPATH)

HFILES = ftserver.h ftserver_data.h ftserver_proc.h ftserver_trans.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

