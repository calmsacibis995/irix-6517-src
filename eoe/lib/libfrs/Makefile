#!smake
#**************************************************************************
# *									  *
# * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
# *									  *
# *  These coded instructions, statements, and computer programs  contain  *
# *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
# *  are protected by Federal copyright law.  They  may  not be disclosed  *
# *  to  third  parties  or copied or duplicated in any form, in whole or  *
# *  in part, without the prior written consent of Silicon Graphics, Inc.  *
# *									  *
# **************************************************************************/

#ident	"$Revision: 1.5 $"

BASELIB=libfrs_32.a
ALTLIBS=libfrs_64.a libfrs_n32_M3.a

include	$(ROOT)/usr/include/make/librootdefs

LLDLIBS=-lc
#
# WARNING 85: definition of XXX preempts that definition in  ...
# WARNING 69: Definition of XXX in .so's interface section appears at least once before
#
#LLDOPTS=-woff 85,69
#DSONAMEOPT=-soname libfrs.so

TARGETS=$(BASELIB) $(ALTLIBS)

default: $(TARGETS)

include $(LIBROOTRULES)

exports install! default
	$(INST_32) -m 444 -src libfrs_32.a libfrs.a
	$(INST_64) -m 444 -src libfrs_64.a libfrs.a
	$(INST_N32) -m 444 -src libfrs_n32_M3.a libfrs.a

