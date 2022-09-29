
default: driver.o driverst.o user ts

driver.o: driver.c common.h
	$(TOOLROOT)/usr/bin/cc -g -DDEBUG -UNDEBUG -D_K32U64 -D_PAGESZ=4096 -D_KERNEL -DSTATIC=static  -D_VCE_AVOIDANCE -DTILE_DATA  -DTLBMOD_BADVADDR_WAR  -D_VCE_AVOIDANCE_DEBUG  -D_TLB_LOOP_LIMIT  -DP0_WAR -D_IRIX5_MIPS4 -D_IRIX5_MIPS3  -D_R5000_BADVADDR_WAR -D_R5000_CDX_WAR -DMIPS4_ISA  -DSW_FAST_CACHE_SYNCH -DSWASH  -DSCACHE_SET_ASSOC=2  -D_R5000_CVT_WAR=1 -D_MTEXT_VFS -DR10000_SPECULATION_WAR -DMH_R10000_SPECULATION_WAR -DSP -DIP32 -DR4000 -DR4000PC -DR4600 -DR4600SC -DTRITON -DR10000 -D_NO_R4000  -I.. -G 0 -elf -mips2 -fullwarn -non_shared -Wc,-jalr -G 0  -nostdinc -I$(ROOT)/usr/include -mips2 -o32 -woff 1685,515,608,658,799,803,852,1048,1233,1499,1068,1209,1505,1506,1692 -c driver.c -o driver.pre.o &&  ld -G 0 -elf -woff 84 -woff 47 -woff 17 -mips2 -o32  -nostdlib -r driver.pre.o -o driver.o

driverst.o: driver.c common.h
	$(TOOLROOT)/usr/bin/cc -D_K32U64 -D_PAGESZ=4096 -D_KERNEL -DSTATIC=static  -D_VCE_AVOIDANCE -DTILE_DATA  -DTLBMOD_BADVADDR_WAR  -D_VCE_AVOIDANCE_DEBUG  -D_TLB_LOOP_LIMIT  -DP0_WAR -D_IRIX5_MIPS4 -D_IRIX5_MIPS3  -D_R5000_BADVADDR_WAR -D_R5000_CDX_WAR -DMIPS4_ISA  -DSW_FAST_CACHE_SYNCH -DSWASH  -DSCACHE_SET_ASSOC=2  -D_R5000_CVT_WAR=1 -D_MTEXT_VFS -DR10000_SPECULATION_WAR -DMH_R10000_SPECULATION_WAR -DSP -DIP32 -DR4000 -DR4000PC -DR4600 -DR4600SC -DTRITON -DR10000 -D_NO_R4000  -I..  -G 8 -elf -mips2 -fullwarn -non_shared  -nostdinc -I$(ROOT)/usr/include -mips2 -o32 -g -woff 1685,515,608,658,799,803,852,1048,1233,1499,1068,1209,1505,1506,1692,835 -c driver.c -o driverst.pre.o &&  $(TOOLROOT)/usr/bin/ld   -G 8 -elf -woff 84 -woff 47 -woff 17 -mips2 -o32  -nostdlib    -r driverst.pre.o -o driverst.o

staticstuff: driverst.o
	cp driverst /var/sysgen/master.d
	cp driverst.sm /var/sysgen/system
	cp driverst.o /var/sysgen/boot

removestatic:
	/bin/rm /var/sysgen/master.d/driverst
	/bin/rm /var/sysgen/system/driverst.sm
	/bin/rm /var/sysgen/boot/driverst.o

newkernel:
	unset ROOT; unset TOOLROOT; autoconfig -fv

nodes:
	mknod deck1 c 271 1
	mknod deck2 c 271 2
	chmod 777 deck1
	chmod 777 deck2

rootnodes:
	mknod /dev/deck1 c 271 1
	mknod /dev/deck2 c 271 2
	chmod 777 /dev/deck1
	chmod 777 /dev/deck2

load:
	/sbin/ml ld -d -c driver.o -p deck -s 271

test:
	xwsh -e user -l -1

unload:	
	/sbin/ml unld 271

user: user.c common.h
	$(TOOLROOT)/usr/bin/cc -g -nostdinc -I$(ROOT)/usr/include -L$(ROOT)/usr/lib -o user user.c -ltserialio

ts: ts.c
	$(TOOLROOT)/usr/bin/cc -g -nostdinc -I$(ROOT)/usr/include -L$(ROOT)/usr/lib -o ts ts.c -ltserialio -ldmedia

REV=mgate1.9

maketar:
	tar cLvf images/${REV}.tar README ${REV} user.c driver.c Makefile common.h tserialio.o driverst driverst.sm ts.c
	(cd images; /bin/rm -f ${REV}.tar.Z ; compress ${REV}.tar; /bin/rm -f ${REV}.tar.Z.uu ; uuencode ${REV}.tar.Z ${REV}.tar.Z > ${REV}.tar.Z.uu)
