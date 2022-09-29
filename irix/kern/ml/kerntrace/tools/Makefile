#!make

# Build commands used for kernel tracing
#
# $Revision: 1.3 $

# Stick to 32 bits so same tools work on all platforms

OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs

# Use the native compiler, linker, and libraries, and suppress target
# include files.
CC	=TOOLROOT= /usr/bin/cc
GCINCS	=
GLDOPTS	=
GLDLIBS	=

# forget
#	"should return a value" (1116)
#	"nested comment" (1009)
#	"pointless comparison of unsigned ..." (1516
LCOPTS+=-woff 1116,1009,1516

# no ld yet
#LDCFILES=cmdlang.o ldlib.o  ldsym.o  load.o ldio.o ldmisc.o link29.o reloc.o
LDOBJECTS=$(LDCFILES:.c=.o)


CFILES	=prord.c tdiff.c tracex.c trascii.c trinsrt.c trord.c
COBJECTS=$(CFILES:.c=.o)

TARGETS	=prord tdiff tracex32 tracex64 trascii trinsert trord

default:$(TARGETS)

include $(COMMONRULES)

install: default

tracex32:	tracex.c
	$(CC) -g -mips3 -o tracex32 tracex.c -lelf

tracex64: 	tracex.c
	$(CC) -64 -g -mips3 -o tracex64 tracex.c -lelf


