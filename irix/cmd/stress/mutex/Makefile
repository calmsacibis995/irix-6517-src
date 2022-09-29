#!smake
#**************************************************************************
#*                                                                        *
#*               Copyright (C) 1986-1994 Silicon Graphics, Inc.           *
#*                                                                        *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*                                                                        *
#**************************************************************************/

CC    = $(TOOLROOT)/usr/bin/cc
OOPTS = -O
CCOPTS = -xansi -fullwarn -nostdinc -I$(ROOT)/usr/include
LDOPTS32 = -nostdlib -L$(ROOT)/usr/lib -L$(ROOT)/lib
LDOPTS64 = -nostdlib -L$(ROOT)/usr/lib64


default: master mutex32 mutex64

master: master.o shmem64.o
	$(CC) -64 $(OOPTS) $(CCOPTS) $(LDOPTS64) master.o shmem64.o -o master

mutex32: shmem32.o mutex32.o shmem.h
	$(CC) $(OOPTS) $(LDOPTS32) mutex32.o shmem32.o -o mutex32

mutex64: shmem64.o mutex64.o
	$(CC) -64 $(OOPTS) $(LDOPTS64) mutex64.o shmem64.o -o mutex64

master.o: master.c shmem.h
	$(CC) -64 $(OOPTS) $(CCOPTS) -c master.c

mutex32.o: mutex.c shmem.h
	$(CC) $(OOPTS) $(CCOPTS) -c mutex.c -o mutex32.o

mutex64.o: mutex.c shmem.h
	$(CC) -64 $(OOPTS) $(CCOPTS) -c mutex.c -o mutex64.o

shmem32.o: shmem.c shmem.h
	$(CC) $(OOPTS) $(CCOPTS) -c shmem.c -o shmem32.o

shmem64.o: shmem.c shmem.h
	$(CC) -64 $(OOPTS) $(CCOPTS) -c shmem.c -o shmem64.o

clean:
	rm -f *.o *~ core

clobber:
	make clean; rm -f mutex32 mutex64 master
