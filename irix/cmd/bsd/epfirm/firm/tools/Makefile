#!smake

# Build cross 29K assembler for FDDI boards
#
# $Revision: 1.17 $

# Stick to 32 bits because the foriegn code is crufty and does not work
# when compiled with 64-bit longs and pointers.
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

ACFILES	=asm29.c code.c dbsym.c doline.c ieee.c macro.c \
		msg.c psop.c reserved.c scan.c sym.c util.c
AOBJECTS=$(ACFILES:.c=.o)

# no ld yet
#LDCFILES=cmdlang.o ldlib.o  ldsym.o  load.o ldio.o ldmisc.o link29.o reloc.o
LDOBJECTS=$(LDCFILES:.c=.o)

C2FCFILES=coff2firm.c
C2FOBJECTS=$(C2FCFILES:.c=.o)

CFILES	=$(ACFILES) $(C2FCFILES)


TARGETS	=as29i coff2firm

default:$(TARGETS)

include $(COMMONRULES)

install: default

as29i:  $(AOBJECTS)
	$(CCF) $(AOBJECTS) $(LDFLAGS) -o $@

ld29i:  $(LOBJECTS)
	$(CCF) $(LOBJECTS) $(LDFLAGS) -o $@

afluff lint:
	${LINT} ${LINTFLAGS} ${CDEFS} ${CINCS} ${C2FCFILES} ${LDLIBS}
	${LINT} ${LINTFLAGS} ${CDEFS} ${CINCS} ${ACFILES} ${LDLIBS}
