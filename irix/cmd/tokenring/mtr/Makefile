# 	Makefile for the applications for mtr network interface 
#
# 	Copyright 1996 Silicon Graphics, Inc.  All rights reserved.
#
#	$Revision: 1.3 $
DEPTH=..
include ${DEPTH}/Makedefs

LCINCS= -I${DEPTH}/include -I.
#LCOPTS= -DDEBUG -g
CFILES	=mtrconfig.c mtr_firm.c mtrsnoop.c mtrmonitor.c

CLIENT	=mtrconfig mtrsnoop mtrmonitor
TARGETS	=${CLIENT}

FILEPERM= -m 644 -u root -g sys
SUID	= -m 04555 -u root -g sys
GUID    = -m 02555 -u root -g sys

default:$(TARGETS)

include ${COMMONRULES}

# ${CLIENT}: $$@.c ${TRLIBTR}

mtrconfig:mtr_firm.o mtrconfig.o
	$(CCF) -o mtrconfig mtr_firm.o mtrconfig.o $(LDFLAGS)

mtrmonitor:mtr_common.o mtrmonitor.o
	$(CCF) -o mtrmonitor mtr_common.o mtrmonitor.o $(LDFLAGS)

mtrsnoop:mtr_common.o mtrsnoop.o
	$(CCF) -o mtrsnoop mtr_common.o mtrsnoop.o $(LDFLAGS)

install :default
	$(INSTALL) -F /usr/etc ${CLIENT}

${_FORCE}:
