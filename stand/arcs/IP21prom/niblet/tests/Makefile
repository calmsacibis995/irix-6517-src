#!pmake
#ROOT=/hosts/yomama.mti/e/root400.exper
#TOOLROOT=/hosts/yomama.mti/e/root400.exper
#COMP_TARGET_ROOT=/hosts/yomama.mti/e/root400.exper
# _RLD_PATH=/hosts/yomama.mti/e/root400.exper
# _RLD_ROOT=/hosts/yomama.mti/e/root400.exper:/
# include ${ROOT}/usr/include/make/commondefs

# Switch these guys and make should proceed appropriately for each
# *** Don't forget to also change the similar ARCH constant in the
# ***  Niblet Makefile!!
#ARCH=T5
ARCH=TFP

# The tests in this directory can be compiled either LONG or SHORT.  
# To compile a test SHORT, simply compile it with SHORT defined.  This
# will decrease the number of times the test loops to a number reasonable
# to execute on sable.  
# Typing 'make' with no target will make everything LONG.  Typeing
# 'make tests' will also make everything LONG.  To make everything SHORT,
# use 'make TEST_LENGTH=SHORT' or 'make tests TEST_LENGTH=SHORT'.   All
# .o's, executables, and disassembled executables go in the directory
# corresponding to their test length - ie: memtest compiled SHORT is
# in SHORT/memtest

#if $(ARCH) == "TFP"
NIB_DEFINES= -DBRINGUP -DSTANDALONE -DEVEREST -DTFP=1 # -DNIBTESTDEBUG  -DNIBTESTDEBUG_BIG
#elif $(ARCH) == "T5"
NIB_DEFINES= -DBRINGUP -DSTANDALONE -DEVEREST -DT5 -DR4000=1
#endif

NIB_OPTS= -non_shared -64 -mips4
NIB_AOPTS= -Wa,-diag
NIB_COPTS=  -cckr
NIB_LOPTS= -non_shared -64
TARGETDIR=${TEST_LENGTH}
CC=$TOOLROOT/usr/bin/cc
AS=$TOOLROOT/usr/bin/as
LD=$TOOLROOT/usr/bin/ld

TEST_LENGTH=LONG
#if $(TEST_LENGTH) == "SHORT"
TEST_DEFINES = -D${TEST_LENGTH}
#endif

NIBPROCS = ${TARGETDIR}/counter ${TARGETDIR}/invalid ${TARGETDIR}/mpardn ${TARGETDIR}/mparup \
  	${TARGETDIR}/mpcount2 ${TARGETDIR}/mpcount3 \
	${TARGETDIR}/mphlock ${TARGETDIR}/mphlock_big \
	${TARGETDIR}/mpintadd ${TARGETDIR}/mpintadd_4_big \
  	${TARGETDIR}/mpmon ${TARGETDIR}/mpslock \
  	${TARGETDIR}/memtest ${TARGETDIR}/memtest_big \
  	${TARGETDIR}/mprove ${TARGETDIR}/mprove_big \
  	${TARGETDIR}/nibfail

.BEGIN:
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

.s: ../nsys.h ../nmsg.h
	${AS} ${NIB_AOPTS} ${NIB_OPTS} ${TEST_DEFINES} ${NIB_DEFINES} ${@F}.s -I. -I.. -o ${TARGETDIR}/${@F}.o
	${LD} ${NIB_LOPTS}  -T 0 -D 4000  -e ENTRY -o ${TARGETDIR}/${@F} ${TARGETDIR}/${@F}.o
	${TOOLROOT}/usr/bin/dis -h ${TARGETDIR}/${@F} > ${TARGETDIR}/${@F}.dis


tests: ${NIBPROCS}


# Not currently used
# SAPROCS = tests/invalid tests/mpardn tests/mparup \
# 	tests/mprove tests/mpslock \
# 	tests/bigmem tests/printtest tests/bigrove \
# 	tests/bighlock tests/bigintadd_4


