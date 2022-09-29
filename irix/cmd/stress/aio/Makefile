#!smake
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/
# $Revision: 1.7 $

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS= runtests aio aio_cb aio_fs aio_fs2 aio_lio aio_append
OPTIMIZER=
LCOPTS+=-g
LCDEFS+=-D_SGI_MP_SOURCE

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/aio $(COMMANDS)

64bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/aio/64bit $(IDB_TAG64) $(COMMANDS)
