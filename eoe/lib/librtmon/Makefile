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

BASELIB=librtmon_32.a
ALTLIBS=librtmon_64.a librtmon_n32_M3.a librtmon_stubs_32.a \
	librtmon_stubs_64.a librtmon_stubs_n32_M3.a librtmon_stubs_32.a 

HEADERS_SUBDIRS=src
EXPORTS_SUBDIRS=src

include	$(ROOT)/usr/include/make/librootdefs

LLDLIBS=-lc
#
# WARNING 85: definition of XXX preempts that definition in  ...
# WARNING 69: Definition of XXX in .so's interface section appears at least once before
#
#LLDOPTS=-woff 85,69
#DSONAMEOPT=-soname librtmon.so

TARGETS=$(BASELIB) $(ALTLIBS)

default: $(TARGETS)

include $(LIBROOTRULES)

exports install! default
	$(INST_32) -m 444 -src librtmon_32.a librtmon.a
	$(INST_64) -m 444 -src librtmon_64.a librtmon.a
	$(INST_N32) -m 444 -src librtmon_n32_M3.a librtmon.a
	$(INST_32) -m 444 -src librtmon_stubs_32.a librtmon_stubs.a
	$(INST_64) -m 444 -src librtmon_stubs_64.a librtmon_stubs.a
	$(INST_N32) -m 444 -src librtmon_stubs_n32_M3.a librtmon_stubs.a
	$(EXPORTS_SUBDIRS_MAKERULE)

headers!
	$(HEADERS_SUBDIRS_MAKERULE)
