#        /d/6.2_compiler/LATEST/usr/bin/cc -xansi -fullwarn -DDEBUG=1 -D_STANDALONE -DSTANDALONE -DMP -DNUMA -DNUMA_PM -DDISCONTIG_PHYSMEM -DPTE_64BIT -DNUMA_MIGRATION -DSN0_USE_BTE -DSABLE_RTL -D_KERNEL -DR10000   -D_PAGESZ=16384 -DIP27 -DPROM   -OPT:unroll_times_max=1 -DSN0 -I../include  -non_shared -elf -64 -G 32 -mips4 -OPT:space -OPT:quad_align_branch_targets=FALSE -OPT:quad_align_with_memops=FALSE -TENV:misalignment=1    -nostdinc -I/b/csm/kudzu/usr/include  -O2  -MDupdate Makedepend.SN0 -woff 1685,515,608,658,799,803,852,1048,1233,1499,1185,1209,1174                                       disctest.c -c -o IP27.O/disctest.o

CFLAGS = -DHUB1_WAR -DDISCTEST -xansi -fullwarn -D_STANDALONE -DSTANDALONE -DMP -DNUMA -DNUMA_PM -DDISCONTIG_PHYSMEM -DPTE_64BIT -DNUMA_MIGRATION -DSN0_USE_BTE -DSABLE_RTL -D_KERNEL -DR10000 -D_PAGESZ=16384 -DPROM -DSN0 -DIP27 -64 -mips4 -nostdinc -I../../include -I$(ROOT)/usr/include -g

#
# Discovery test
#

LINKFILES=discover.c discover.h hub.h ip27prom.h net.h libasm.h router.h
SHELL=/bin/sh

all: $(LINKFILES) disctest

$(LINKFILES):
	for i in $(LINKFILES); do ln -sf ../$$i .; done

DISCTEST_OBJECTS = \
	disctest.o discover.o

disctest: ${DISCTEST_OBJECTS}
	cc -mips4 -g -o disctest ${DISCTEST_OBJECTS}

clean:
	rm -f $(LINKFILES) *.o core disctest
