CC		= ${TOOLROOT}/usr/bin/DCC
CFLAGS  	= -O -I${ROOT}/usr/include -I${ROOT}/usr/include/sys
DEBUG_CFLAGS  	= -w -g -O0 -I${ROOT}/usr/include -I${ROOT}/usr/include/sys -L${ROOT}/usr/lib -L${TOOLROOT}/usr/lib -L{TOOLROOT}/usr/lib/DCC
MIPS_CFLAGS  	= -systype bsd43

all: genSB.irix

genSB.irix:      sbkiller.C  sbkiller.h
	$(CC) $(DEBUG_CFLAGS) sbkiller.C  -o genSB.irix 

fast:	sbkiller.c
	$(CC) $(DEBUG_CFLAGS) sbkiller.C  -o genSB.irix 

	
