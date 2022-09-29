#**************************************************************************
#*                                                                        *
#*               Copyright (C) 1994, Silicon Graphics, Inc.               *
#*                                                                        *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*                                                                        *
#**************************************************************************

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs

# If you are not using the driver on an IP27 based machine then you need
# to replace the system architecture type (SYS_ARCH_TYPE) parameters below
# with one of the following definitions:
#
# -DSN0     -DIP27 -DR10000 (default)
# -DEVEREST -DIP25 -DR10000
# -DEVEREST -DIP19 -DR4000
# -DEVEREST -DIP21 -DR8000

SYS_ARCH_TYPE= -DSN0 -DIP27 -DR10000

CFLAGS= -D_KERNEL -DSTATIC=static -D_PAGESZ=16384 -D_MIPS3_ADDRSPACE \
	$(SYS_ARCH_TYPE) -DMP -G 8 -non_shared -elf -xansi -64 -mips3 \
	-TENV:kernel -TENV:misalignment=1 -OPT:space -DFRSDRV_DEBUG

TARGETS= frsdrv.o
CFILES=  main.c

default: ${TARGETS}

include ${COMMONRULES}

frsdrv.o: $(OBJECTS)
	ld -64  -r -o frsdrv.o $(OBJECTS)


install: ${TARGETS}
	cp frsdrv.o $(ROOT)/var/sysgen/boot/frsdrv.o
	cp frsdrv.spec $(ROOT)/var/sysgen/master.d/frsdrv
	./add_to_irix.sm
	/etc/autoconfig -vf -p $(TOOLROOT) -d $(ROOT)/var/sysgen

clobber:	clean

clean:
		rm -f *.o

