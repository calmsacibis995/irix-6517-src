#!smake
#
# Makefile for libnanothread.
#
#ident	"$Revision: 1.6 $"

DEPTH=..
ASFILES=uswtch.s
CFILES=nanothread.c

include $(ROOT)/usr/include/make/libleafdefs
include $(LIBLEAFRULES)

TARGETS += nanothread.h nanothread.o

../autoheader/ugenassym: ../autoheader/ugenassym.c
	cd ../autoheader; ${MAKE} ugenassym

nanothread.h: nanothread.template.h ../autoheader/ugenassym
	../autoheader/ugenassym < nanothread.template.h > nanothread.h

uswtch.o: uswtch.s nanothread.h
	rm -f uswtch.h
	echo "#define neststrt 0x4" > uswtch.h
	${ASF} ${VERSION_CDEFS} -o $*.o $<
	rm -f uswtch.h
	echo "#define neststrt `dis uswtch.o | ./assem_hack - _resume_nid_nest`" > uswtch.h
	${ASF} ${VERSION_CDEFS} -o $*.o $<
	rm -f uswtch.h

uswtch.h:
	touch uswtch.h
