#	@(#)Makefile	8.15 (Berkeley) 7/28/94

PROG=	dbtest
OBJS=	dbtest.o
LIBDB=	libdb.a

# Uncomment the STAT line get hash and btree statistical use info.  This
# also forces ld to load the btree debug functions for use by gdb, which
# is useful.  The db library has to be compiled with -DSTATISTICS as well.
INC=	-I${PORTDIR}/include -I${PORTDIR}
OORG=	-g
#STAT=	-DSTATISTICS
CFLAGS=-D__DBINTERFACE_PRIVATE -DDEBUG ${STAT} ${OORG} ${INC}

dbtest: ${OBJS} ${PORTDIR}/${LIBDB}
	${CC} -o $@ ${OBJS} ${OORG} ${PORTDIR}/${LIBDB}

clean:
	rm -f dbtest.core gmon.out ${OBJS} ${PROG} t1 t2 t3

${OBJS}: Makefile
