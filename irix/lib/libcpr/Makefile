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

BASELIB=libcpr_n32_M3.a
ALTLIBS=libcpr_n32_M4.a libcpr_64_M3.a libcpr_64_M4.a \
	libcpr_32_M2.a

DSOEXPORTS=common.exports
DSOEXPORTS_N32=common.exports
DSOEXPORTS_64_M3=common.exports


WANTPARALLEL=yes-please

include	$(ROOT)/usr/include/make/librootdefs

#EXPORTS=-exported_symbol ckpt_setup,ckpt_create,ckpt_restart,ckpt_remove,ckpt_stat,ckpt_error,ckpt_notice,ckpt_reach_sync_point,ckpt_type_str,rev_to_str,atol_type,ckpt_restart_is_client,cpr_line,cpr_file,cpr_flags,cpr_debug,cr,cpr_share_prop_table,pipe_rc,cpr_proc_prop_table

DSONAMEOPT=-soname libcpr.so

LLDLIBS+= -lc

default: $(TARGETS)

include $(LIBROOTRULES)

install!headers exports

headers!
	$(INSTALL) -F /usr/include -m 444 -src src/ckpt.h ckpt.h



exports!default headers
	$(INST_64)  -m 444 -src libcpr_64_M3.so     libcpr.so
	$(INST_64_M4) -m 444 -src libcpr_64_M4.so libcpr.so
	$(INST_N32) -m 444 -src libcpr_n32_M3.so libcpr.so
	$(INST_N32_M4) -m 444 -src libcpr_n32_M4.so libcpr.so
	$(I_32_M2) $(INSTALL) -F /lib -m 444 -src libcpr_32_M2.so libcpr.so





