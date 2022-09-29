#!smake
#
# Makefile for clshm tests
#

include $(ROOT)/usr/include/make/commondefs

LCOPTS = -g

LCINCS = -I $(ROOT)/../irix/kern -I $(ROOT)/usr/include -I $(ROOT)/../irix/lib/libclshm/src

LLDINCS = -L $(ROOT)/usr/lib32

LLDINCS64 = -L $(ROOT)/usr/lib64

LIBS = -lclshm

OBJECTS = shmtest.o shmdump.o clshm_barrier.o shm_exhaust_keys.o \
	shmsmall.o shmrm.o shmzero.o shmlen.o shmauto.o
TARGETS = shmtest shmdump shmsmall shmrm shmzero shmlen shmauto

default: $(TARGETS)

include $(COMMONRULES)

# CC = /toolroots/kudzu/usr/bin/cc -64
# LCFLAGS = -DDEBUG $(LCOPTS) $(LCINCS) -64
# CFLAGS = $(LCINCS) $(LLDINCS64) -64 -g	# -64 builds mips4 by default
CFLAGS = $(LCINCS) $(LLDINCS) -n32 -g	# MPI's env. is mostly n32

clshm_barrier.o: clshm_barrier.h clshm_barrier.c
	$(CC) $(CFLAGS) -c clshm_barrier.c

shmtest.o: clshm_barrier.h shmtest.c
	$(CC) $(CFLAGS) -c shmtest.c

shmtest: shmtest.o clshm_barrier.o
	$(CC) $(CFLAGS) -o shmtest shmtest.o clshm_barrier.o $(LIBS)

shmdump.o: shmdump.c
	$(CC) $(CFLAGS) -c shmdump.c

shmdump: shmdump.o
	$(CC) $(CFLAGS) -o shmdump shmdump.o $(LIBS)

shmsmall.o: shmsmall.c
	$(CC) $(CFLAGS) -c shmsmall.c

shmsmall: shmsmall.o
	$(CC) $(CFLAGS) -o shmsmall shmsmall.o $(LIBS)

shmrm.o: shmrm.c
	$(CC) $(CFLAGS) -c shmrm.c

shmrm: shmrm.o
	$(CC) $(CFLAGS) -o shmrm shmrm.o $(LIBS)

shmzero.o: shmzero.c
	$(CC) $(CFLAGS) -c shmzero.c

shmzero: shmzero.o
	$(CC) $(CFLAGS) -o shmzero shmzero.o $(LIBS)

shmauto.o: shmauto.c
	$(CC) $(CFLAGS) -c shmauto.c

shmauto: shmauto.o
	$(CC) $(CFLAGS) -o shmauto shmauto.o $(LIBS)

shmlen.o: shmlen.c
	$(CC) $(CFLAGS) -c shmlen.c

shmlen: shmlen.o
	$(CC) $(CFLAGS) -o shmlen shmlen.o $(LIBS)

shm_exhaust_keys.o: shm_exhaust_keys.c
	$(CC) $(CFLAGS) -c shm_exhaust_keys.c

shm_exhaust_keys: shm_exhaust_keys.o
	$(CC) $(CFLAGS) -o shm_exhaust_keys shm_exhaust_keys.o $(LIBS)

