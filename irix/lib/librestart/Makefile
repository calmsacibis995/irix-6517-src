#!smake
#**************************************************************************
# *									  *
# * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
# *									  *
# *  These coded instructions, statements, and computer programs  contain  *
# *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
# *  are protected by Federal copyright law.  They  may  not be disclosed  *
# *  to  third  parties  or copied or duplicated in any form, in whole or  *
# *  in part, without the prior written consent of Silicon Graphics, Inc.  *
# *									  *
# **************************************************************************/

#ident	"$Revision: 1.10 $"

BASELIB=librestart_n32_M3.a
ALTLIBS=librestart_64.a 

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

WANTPARALLEL=yes-please

NO_STARTOPT=

include	$(ROOT)/usr/include/make/librootdefs


DSONAMEOPT=-soname librestart.so
DSOSTARTOPT_N32=-check_registry ./so_locations
DSOSTARTOPT_64=-check_registry ./so_locations

#LDDSOOPTS_N32+=$(EXPORTS)
#LDDSOOPTS_64+=$(EXPORTS)
LD_NO_UNRESOLVED=

default: $(TARGETS)

include $(LIBROOTRULES)

install!headers exports

headers!

exports!default
	$(INST_64)  -m 444 -src librestart_64.so     $(IDB_TAG64) librestart.so
	$(INST_N32) -m 444 -src librestart_n32_M3.so $(IDB_TAG32) librestart.so
