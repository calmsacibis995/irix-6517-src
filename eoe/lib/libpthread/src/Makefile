#!smake

DEPTH=.

SM=:

include $(ROOT)/usr/include/make/libleafdefs

CFILES=	\
	cncl.o \
	common.c \
	cvattr.c \
	cv.c \
	delay.c \
	event.c \
	fork.c \
	intr.c \
	key.c \
	libcthread.c \
	lock.c \
	mtxattr.c \
	mtx.c \
	once.c \
	pt.c \
	ptattr.c \
	ptdbg.c \
	rwlattr.c \
	rwl.c \
	sem.c \
	sig.c \
	stk.c \
	vp.c

ASFILES= \
	asm.s

include $(LIBLEAFRULES)
