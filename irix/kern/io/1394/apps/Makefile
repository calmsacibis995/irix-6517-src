#!smake
# Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
#
# Makefile for mips-specific network interfaces.
#
# $Revision: 1.1 $

include commondefs

TARGETS =				\
		camstat			\
		check_interest		\
		fwioctl			\
		gain_interest		\
		isoxmtframes		\
		isoxmtstill		\
		quadlock		\
		quadwrite		\
		example			\
		lose_interest		\
		quadread


#
# Rules
#

default:	$(TARGETS)

.c:
	$(CC) $@.c -o $@ -nostdinc -I $(ROOT)/usr/include -I ../ -L$(ROOT)/usr/lib
include commonrules
