#!pmake

# This Makefile makes a number of different niblet executables.  For running
# niblet from the bootprom, use either the prom_niblet_long or prom_niblet_short
# targets.  The only difference between these is that with prom_niblet_long you
# get long versions of tests, and with prom_niblet_short you get short ones.
# For running from the prom, you probably want to use prom_niblet_long.
# In both of the prom cases, the makefile creates a 4 .o files that get
# get put in either PROM_SHORT or PROM_LONG.  These are: dcob.o, nib_procs.o,
# nib_code.o, and nib_conf.o.  When you type 'make ip21prom'
# in the directory above this one, the makefile there will reach into this
# directory and grab the .o's from PROM_LONG.
#
# Running niblet without the bootprom is called running RAW.  When you type
# 'make raw_niblet_short' this makefile will generate two objects: raw_niblet
# and raw_nibdata.  Currently raw_niblet is intended for a simulation environment
# and it may have stuff in it that depends on special features of our simulator.
# For example, to stop the simulation I am using the break 1023, 1023 instruction
# to halt Sable and the TFP simulator.

#ROOT=/hosts/yomama.mti/e/root400.exper
#TOOLROOT=/hosts/yomama.mti/e/root400.exper
#COMP_TARGET_ROOT=/hosts/yomama.mti/e/root400.exper
#_RLD_PATH=/hosts/yomama.mti/e/root400.exper/lib/rld
#_RLD_ROOT=/hosts/yomama.mti/e/root400.exper:/
### include ${ROOT}/usr/include/make/commondefs

CC=$(TOOLROOT)/usr/bin/cc
AS=$(TOOLROOT)/usr/bin/as
LD=$(TOOLROOT)/usr/bin/ld
HOST_CC = TOOLROOT= /usr/bin/cc

# DEFINES is intentionally left empty.  The idea is the user can define
# additional DEFINES on the command line.  eg: make raw_niblet_short DEFINES=-DQUANTUM_SHORT -DPRINT_IN_TLB_REFILL
DEFINES=
NIB_TEST_DEFINES = # -DNIBTESTDEBUG
NIB_DEFINES= -DBRINGUP -DSTANDALONE -DEVEREST -DTFP=1 ${DEFINES} # -DDCOBDEBUG # -DNIBDEBUG -DCOBDEBUG -DQUANTUM_SHORT -DNO_PREEMPTION -DPRINTSCHED # -DVERBOSE
NIB_OPTS= -non_shared -64 -mips4
NIB_AOPTS= -Wa,-diag
NIB_COPTS=  -cckr
NIB_LOPTS= -non_shared -64
HOST_DEFINES= ${DEFINES}
TOOLS=TOOLS


RAW_NIBDATA_LINK_ADDRESS= a800000000111000


NIBPROCS = ${TESTDIR}/counter ${TESTDIR}/invalid ${TESTDIR}/mpardn ${TESTDIR}/mparup \
  	${TESTDIR}/mpcount2 ${TESTDIR}/mpcount3 \
	${TESTDIR}/mphlock ${TESTDIR}/mphlock_big \
	${TESTDIR}/mpintadd ${TESTDIR}/mpintadd_4_big \
  	${TESTDIR}/mpmon ${TESTDIR}/mpslock \
  	${TESTDIR}/memtest ${TESTDIR}/memtest_big \
	${TESTDIR}/mprove ${TESTDIR}/mprove_big \
	${TESTDIR}/nibfail

TEXT_SECTION= a800000000100000  #TEXT_SECTION= 80100000
#DATA_SECTION= a800000000104000 #DATA_SECTION= 80104000


#ifmake prom_niblet_long
TYPE=PROM
TEST_LENGTH=LONG

#elifmake prom_niblet_short
TYPE=PROM
TEST_LENGTH=SHORT

#elifmake raw_niblet_short
TYPE=RAW
TEST_LENGTH=SHORT

#else
TYPE=PROM
TEST_LENGTH=LONG
#endif


TARGETDIR=$(TYPE)_$(TEST_LENGTH)
TESTDIR=tests/$(TEST_LENGTH)


.BEGIN:
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi
	@if test ! -d ${TOOLS}; then mkdir ${TOOLS}; fi
	cd tests; make tests TEST_LENGTH=${TEST_LENGTH} ${NIB_TEST_DEFINES}

default: prom_niblet_long
	

prom_niblet_long: ${TARGETDIR}/nib_procs.o ${TARGETDIR}/dcob.o ${TARGETDIR}/nib_code.o ${TARGETDIR}/nib_conf.o

prom_niblet_short: ${TARGETDIR}/nib_procs.o ${TARGETDIR}/dcob.o ${TARGETDIR}/nib_code.o ${TARGETDIR}/nib_conf.o

raw_niblet_short: ${TARGETDIR}/raw_niblet ${TARGETDIR}/raw_nibdata





#################################################################
# Make ${TARGETDIR}/nib_code.o
# 
#################################################################
${TARGETDIR}/nib_code.o: ${TARGETDIR}/nib_code.c
	@echo "*** CC nib_code.c into nib_code.o"
	${CC} ${NIB_LOPTS} -G 0 -c -g -I ${TARGETDIR}/nib_code.c -o ${TARGETDIR}/nib_code.o

${TARGETDIR}/nib_code.c: ${TARGETDIR}/prom_niblet ${TOOLS}/nextract
	@echo "*** nextract prom_niblet into nib_code.c"
	${TOOLS}/nextract ${TARGETDIR}/prom_niblet > ${TARGETDIR}/nib_code.c

${TARGETDIR}/prom_niblet: ${TARGETDIR}/prom_niblet.o ${TARGETDIR}/nib_lib.o
	@echo "*** ld prom_niblet.o & nib_lib.o into raw_niblet"
	${LD} -G 0 -rom -allow_jump_at_eop ${NIB_LOPTS} -T ${TEXT_SECTION} ${TARGETDIR}/prom_niblet.o ${TARGETDIR}/nib_lib.o -o ${TARGETDIR}/prom_niblet
	$(TOOLROOT)/usr/bin/dis -h ${TARGETDIR}/prom_niblet > ${TARGETDIR}/prom_niblet.dis
#	$(TOOLROOT)/usr/bin/nm -Bnx prom_niblet >prom_niblet.nm # Margie removed because nm doesn't work FIX

${TARGETDIR}/prom_niblet.o: niblet.s niblet.h locks.h nsys.h nmsg.h passfail.h ../niblet_debug.h
	@echo "*** Assemble niblet.s into prom_niblet.o"
	${AS} -I -I. -I.. -I../../include -I$(ROOT)/usr/include \
		${NIB_AOPTS} ${NIB_OPTS} ${NIB_DEFINES} niblet.s -o ${TARGETDIR}/prom_niblet.o


#################################################################
# Make ${TARGETDIR}/dcob.o                                      #
#################################################################


${TARGETDIR}/dcob.o: dcob.c cob.h niblet.h ${TARGETDIR}/nib_procs.h
	@echo "*** CC dcob.c into dcob"
	${CC} -G 0 -c -g -I -I. -I${TARGETDIR} -I$(ROOT)/usr/include ${NIB_COPTS} ${NIB_OPTS} ${NIB_DEFINES} -DCOOKED_NIBLET\
		-D printf=loprintf -o ${TARGETDIR}/dcob.o dcob.c

#################################################################
# Make nextract
#################################################################
${TOOLS}/nextract: nextract.c cob.h
	@echo "*** cc nextract.c into nextract"
	${CC} ${NIB_DEFINES} -g nextract.c -nostdinc -nostdlib -I -I${ROOT}/usr/include -L${ROOT}/usr/lib -lelf -o ${TOOLS}/nextract


#################################################################
# Targets used for both prom_niblet and raw_niblet
#################################################################



############################
# ${TARGETDIR}/nib_lib.o   #
############################

$(TARGETDIR)/nib_lib.o: nib_lib.s
	@echo "*** Assemble nib_lib.s => nib_lib.o"
	${AS} -I.. -I$(ROOT)/usr/include \
		${NIB_AOPTS} ${NIB_OPTS} ${NIB_DEFINES} nib_lib.s -o ${TARGETDIR}/nib_lib.o
	@echo "done with lib_lib.o"


############################
# ${TARGETDIR}/nib_procs.o #
############################

#if $(TYPE) == "PROM"
${TARGETDIR}/nib_procs.o: ${TARGETDIR}/nib_procs.c 
	@echo "*** TOOLROOT/CC nib_procs.c into nib_procs.o"
	${CC} ${NIB_LOPTS} ${NIB_COPTS} -G 0 -c -g -I ${TARGETDIR}/nib_procs.c -o ${TARGETDIR}/nib_procs.o
#elif ${TYPE} == "RAW"
${TARGETDIR}/nib_procs.o: ${TARGETDIR}/nib_procs.c 
	@echo "*** CC nib_procs.c into nib_procs.o"
	${HOST_CC} ${NIB_COPTS} -G 0 -c -g -I ${TARGETDIR}/nib_procs.c -o ${TARGETDIR}/nib_procs.o
#endif

${TARGETDIR}/nib_procs.c ${TARGETDIR}/nib_procs.h: cob.h ${TOOLS}/scob ${NIBPROCS}
	@echo "*** scob NIBPROCS "
	${TOOLS}/scob -o ${TARGETDIR}/nib_procs ${NIBPROCS}

############################
# ${TOOLS}/scob            #
############################

${TOOLS}/scob: scob.c cob.h niblet.h
	@echo "*** CC scob.c into scob"
#	${HOST_CC} -g scob.c ${NIB_COPTS} ${NIB_DEFINES} -I -I. -I/usr/include  -L/usr/lib -fullwarn -o ${TOOLS}/scob -lelf
	${CC} -g scob.c ${NIB_COPTS} ${NIB_DEFINES} -nostdinc -nostdlib -I -I. -I${ROOT}/usr/include  -L${ROOT}/usr/lib -o ${TOOLS}/scob -lelf


############################
# ${TARGETDIR}/nib_conf.o  #
############################

#if $(TYPE) == "PROM"
${TARGETDIR}/nib_conf.o: nib_conf.c ${TARGETDIR}/nib_procs.h niblet.h
	@echo "*** CC nib_conf.c into nib_conf.o"
	${CC} ${NIB_LOPTS} -G 0 -c -g -I -I${TARGETDIR} nib_conf.c -o ${TARGETDIR}/nib_conf.o
#elif $(TYPE) == "RAW"
${TARGETDIR}/nib_conf.o: nib_conf.c ${TARGETDIR}/nib_procs.h niblet.h
	@echo "*** CC nib_conf.c into nib_conf.o"
	${HOST_CC} -G 0 -c -g -I nib_conf.c -I${TARGETDIR} -o ${TARGETDIR}/nib_conf.o
#endif


#################################################################
# Targets used for raw_niblet only
#################################################################

${TARGETDIR}/raw_dcob: ${TARGETDIR}/raw_dcob.o ${TARGETDIR}/nib_conf.o ${TARGETDIR}/nib_procs.o
	@echo "*** Making raw_dcob ***"
	${HOST_CC} -g ${TARGETDIR}/raw_dcob.o  ${TARGETDIR}/nib_conf.o ${TARGETDIR}/nib_procs.o -L/usr/lib -fullwarn -o ${TARGETDIR}/raw_dcob -lelf
	@echo "*** Done Making raw_dcob ***"

${TARGETDIR}/raw_dcob.o: dcob.c cob.h niblet.h ${TARGETDIR}/nib_procs.h
	@echo "*** Making raw_dcob.o ***"
	${HOST_CC} -g -c dcob.c ${NIB_COPTS} ${NIB_DEFINES} -DTFP -DRAW_NIBLET -Dloprintf=printf -I -I. -I${TARGETDIR} -I/usr/include -fullwarn -o ${TARGETDIR}/raw_dcob.o # -DDEBUG
	@echo "*** Done Making raw_dcob.o ***"

# ${TARGETDIR}/raw_nibdata.s ${TARGETDIR}/raw_programs.s ${TARGETDIR}/raw_pg_tbl.s ${TARGETDIR}/raw_proc_tbl.s: ${TARGETDIR}/raw_dcob
${TARGETDIR}/raw_nibdata.s: ${TARGETDIR}/raw_dcob
	cd $(TARGETDIR)
	./raw_dcob 0

${TARGETDIR}/raw_nibdata: ${TARGETDIR}/raw_nibdata.s # ${TARGETDIR}/raw_programs.s ${TARGETDIR}/raw_pg_tbl.s ${TARGETDIR}/raw_proc_tbl.s
	cd ${TARGETDIR}
	${CC} ${NIB_COPTS} ${NIB_OPTS} -G 0 -c -g raw_nibdata.s raw_programs.s raw_pg_tbl.s raw_proc_tbl.s
	# The DATA link address *must* be the same as NIBLET_RAWDATA_ADDR in niblet.h!!!
	${LD} -G 0 -allow_jump_at_eop ${NIB_LOPTS} -T a800000000000000 -D ${RAW_NIBDATA_LINK_ADDRESS} raw_nibdata.o raw_programs.o raw_pg_tbl.o raw_proc_tbl.o -o raw_nibdata


${TARGETDIR}/raw_niblet.o: niblet.s niblet.h locks.h nsys.h nmsg.h passfail.h
	@echo "*** Assemble niblet.s into raw_niblet.o"
	${AS} -I -I. -I.. -I../../include -I$(ROOT)/usr/include \
		${NIB_AOPTS} ${NIB_OPTS} ${NIB_DEFINES} -DRAW_NIBLET niblet.s -o ${TARGETDIR}/raw_niblet.o

${TARGETDIR}/raw_niblet: ${TARGETDIR}/raw_niblet.o ${TARGETDIR}/nib_procs.o ${TARGETDIR}/nib_lib.o ${TARGETDIR}/raw_nibdata
	@echo "*** ld raw_niblet.o & nib_lib.o into raw_raw_niblet"
	${LD} -G 0 -rom -allow_jump_at_eop ${NIB_LOPTS} -T ${TEXT_SECTION}  -e nib_text_entry ${TARGETDIR}/raw_niblet.o ${TARGETDIR}/nib_lib.o -o ${TARGETDIR}/raw_niblet
	$(TOOLROOT)/usr/bin/dis -h ${TARGETDIR}/raw_niblet > ${TARGETDIR}/raw_niblet.dis
#	$(TOOLROOT)/usr/bin/nm -Bnx raw_niblet >raw_niblet.nm









clean:
	rm -f *.o sa_procs.* nib_procs.* nib_code.* ../${TARGETDIR}/nib_*.o \
		../${TARGETDIR}/dcob.o tests/*.o *.dis tests/*.dis ${NIBPROCS} \
		${SAPROCS}














nib_csu.o: nib_csu.s
	@echo "*** AS nib_csu.s into nib_csu.o"
	${AS} -I. -I.. -I../../include -I$(ROOT)/usr/include \
		${NIB_AOPTS} ${NIB_OPTS} ${NIB_DEFINES} nib_csu.s -o nib_csu.o


sanib: nib_csu.o nib_lib.o sa_procs.o ../${TARGETDIR}/dcob.o \
		../${TARGETDIR}/nib_code.o ../${TARGETDIR}/pod_io.o ../${TARGETDIR}/pod_ioasm.o \
		../${TARGETDIR}/libasm.o ../${TARGETDIR}/pod_cache.o nib_util.o \
		sa_conf.o ../${TARGETDIR}/epcuart.o ../${TARGETDIR}/ccio.o
	${LD} -T ${TEXT_SECTION} ${NIB_LOPTS} -n -N -o sanib nib_csu.o ../${TARGETDIR}/ccio.o \
		sa_procs.o ../${TARGETDIR}/dcob.o ../${TARGETDIR}/nib_code.o \
		../${TARGETDIR}/pod_io.o ../${TARGETDIR}/libasm.o \
		../${TARGETDIR}/pod_cache.o nib_util.o ../${TARGETDIR}/pod_ioasm.o \
		../${TARGETDIR}/epcuart.o sa_conf.o -e ENTRY
	$(CONVERT) -f s3rec sanib >sanib.s3
	$(TOOLROOT)/usr/bin/nm -Bnx sanib >sanib.nm
	$(TOOLROOT)/usr/bin/dis sanib >sanib.dis
	size -x sanib


# sa_procs.c: ${SAPROCS} cob.h scob
# 	@echo "*** scob SAPROCS"
# 	./scob -o sa_procs ${SAPROCS}
# 
# 
# sa_procs.o: sa_procs.c 
# 	@echo "*** cc sa_procs.c into sa_procs.h"
# 	cc ${NIB_LOPTS} -G 0 -c -g -I sa_procs.c -o sa_procs.o
# 
# 
# sa_conf.o: sa_conf.c sa_procs.h niblet.h
# 	@echo "*** cc sa_conf.c into sa_conf.o"
# 	cc ${NIB_LOPTS} -G 0 -c -g -I sa_conf.c -o sa_conf.o
# 
# 
# nib_util.o: nib_util.c
# 	@echo "*** cc nib_util.c into nib_util.o"
# 	cc ${NIB_COPTS} ${NIB_OPTS} ${NIB_DEFINES} -G 0 -c -g -I nib_util.c -o nib_util.o
# 
# 
# 

















#raw_nib_code.c: raw_niblet nextract
#	@echo "*** nextract raw_niblet into raw_nib_code.c"
#	./nextract raw_niblet > raw_nib_code.c
#
#../${TARGETDIR}/raw_nib_code.o: raw_nib_code.c
#	@echo "*** CC raw_nib_code.c into raw_nib_code.o"
#	${CC} ${NIB_LOPTS} -G 0 -c -g -I raw_nib_code.c -o ../${TARGETDIR}/raw_nib_code.o
#
# Why doesn't this work? Change INFO in dcob.c to 6000 when you get it to work?
#	${LD} -G 0 -rom -allow_jump_at_eop ${NIB_LOPTS} -D 6000 raw_proc_tbl.o raw_programs.o raw_pg_tbl.o -o raw_nibdata


#
#
#	${CC} ${NIB_OPTS} ${NIB_COPTS} -c -g programs.s -o programs.o
#	${LD} -G 0 -rom -allow_jump_at_eop ${NIB_LOPTS} -T 6000 programs.o -o raw_nibdata
# Lilian's library
# /hosts/chuchai/usr/people/lilian/d1root/v4.00/mips32/libelf/libelf.a
# Lilian's source
# /hosts/chuchai/usr/people/lilian/d1root/v4.00/libelf/common





# $TOOLROOT/usr/bin/dump -o -v raw_nibdata
# SOmetimes when I use -D without -rom the linker will give me a message like
# this
#   WARNING 41: -D value rounded to 0xa800000000111000.
#
# This is because the data section's virtual synonym must be the same
# as where it gets loaded into memory, and somehow this gets encoded in the
# load information.
# When you use -D with -rom, you currently get an error message.  This
# is because currently -rom gives you only one section, a text setion.
# For now just use -T.  THey are fixing this.





