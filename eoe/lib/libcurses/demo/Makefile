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
#
#ident   "$Revision $"
#
# Common Definitions
#
include	$(ROOT)/usr/include/make/commondefs

#
# Compile Time Options
#
LLDLIBS= -lcurses

.c:
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ 

default: aliens rain menu worms 

#
# Specific Target/Rules follow
#
install: default
