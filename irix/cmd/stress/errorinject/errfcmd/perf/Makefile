CFLAGS=-64 -I../..
ASFLAGS=-64 -I../..

all: iter ctest

SRCS=iter.c testme.s ctest.c cac_t.s

iter:	iter.o testme.o
	cc -64 -o iter iter.o testme.o

ctest:	ctest.o cac_t.o
	cc -64 -o cact ctest.o cac_t.o
