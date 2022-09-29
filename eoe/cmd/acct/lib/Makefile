#!smake
#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"$Revision: 1.6 $"

include	$(ROOT)/usr/include/make/commondefs

TARGETS = a.a
LCINCS  = -I..

FILES= \
	pnpsplit.o \
	copyn.o \
	devtolin.o \
	expand.o \
	lintodev.o \
	namtouid.o \
	tmless.o \
	tmsecs.o \
	uidtonam.o \
	substr.o

default:	$(TARGETS)

$(TARGETS): $(FILES)
	$(AR) crs $(TARGETS) $(FILES)

include $(COMMONRULES)
