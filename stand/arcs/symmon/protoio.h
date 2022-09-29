/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

#ident "$Revision: 1.3 $"

/*
 * protoio.h (prom version) -- protocol io interface file
 * used to adapt protocol code to prom or unix environment
 */
#define	PUTC(c, fd)	putc(c, (int)fd)
#define	GETC(fd)	getc(fd)
#define	PUTFLUSH(fd)
#define	PINIT(fd)
