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

#ident	"$Revision: 1.1 $"

BASELIB=libtserialio_32.a
ALTLIBS=libtserialio_64.a libtserialio_n32_M3.a

include	$(ROOT)/usr/include/make/librootdefs

DSONAMEOPT=-soname libtserialio.so
LLDLIBS=-lc


default: $(TARGETS)

include $(LIBROOTRULES)

install!headers exports

headers!
	$(INSTALL) -F $(INSTINC) -m 444 -src inc/tserialio.h tserialio.h

exports!default
	$(INST_32)  -m 444 -src libtserialio_32.so     libtserialio.so
	$(INST_64)  -m 444 -src libtserialio_64.so     libtserialio.so
	$(INST_N32) -m 444 -src libtserialio_n32_M3.so libtserialio.so
