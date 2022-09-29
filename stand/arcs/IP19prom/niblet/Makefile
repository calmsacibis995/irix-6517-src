include ${ROOT}/usr/include/make/commondefs

NIBPROCS = tests/counter tests/invalid tests/mpardn tests/mparup \
	tests/mpcount2 tests/mpcount3 tests/mphlock tests/mpintadd \
	tests/mpintadd_4 tests/mpmon tests/mpslock \
	tests/memtest tests/bigmem tests/printtest tests/bigrove \
	tests/bighlock tests/bigintadd_4 tests/mprove \
	tests/nibfail

SAPROCS = tests/invalid tests/mpardn tests/mparup \
	tests/mprove tests/mpslock \
	tests/bigmem tests/printtest tests/bigrove \
	tests/bighlock tests/bigintadd_4

NIB_OPTS= -DBRINGUP -DSTANDALONE -DEVEREST -DR4000 -non_shared -coff
NIB_AOPTS= ${NIB_OPTS} -Wa,-r4000 -Wa,-mips3 -32bit
NIB_COPTS= ${NIB_OPTS} -cckr

.s:     nsys.h nmsg.h
	as -non_shared -coff -Wa,-r4000 -Wa,-mips3 -32bit $*.s -I. -o $*.o
	ld -non_shared $*.o -coff -N -T 0 -D 4000 -e ENTRY -o $*
	$(TOOLROOT)/usr/bin/dis -h $* >$*.dis

niblet: ../IP19.O/nib_procs.o ../IP19.O/dcob.o ../IP19.O/nib_code.o \
		../IP19.O/nib_conf.o
	echo Done

sanib: nib_csu.o nib_lib.o sa_procs.o ../IP19.O/dcob.o \
		../IP19.O/nib_code.o ../IP19.O/pod_io.o ../IP19.O/pod_ioasm.o \
		../IP19.O/libasm.o ../IP19.O/pod_cache.o nib_util.o \
		sa_conf.o ../IP19.O/epcuart.o ../IP19.O/ccio.o
	ld -T 80100000 -non_shared -coff -n -N -o sanib nib_csu.o ../IP19.O/ccio.o \
		sa_procs.o ../IP19.O/dcob.o ../IP19.O/nib_code.o \
		../IP19.O/pod_io.o ../IP19.O/libasm.o \
		../IP19.O/pod_cache.o nib_util.o ../IP19.O/pod_ioasm.o \
		../IP19.O/epcuart.o sa_conf.o -e ENTRY
	$(CONVERT) -f s3rec sanib >sanib.s3
	$(TOOLROOT)/usr/bin/nm -Bnx sanib >sanib.nm
	$(TOOLROOT)/usr/bin/dis sanib >sanib.dis
	size -x sanib

clean:
	rm -f *.o sa_procs.* nib_procs.* nib_code.* ../IP19.O/nib_*.o \
		../IP19.O/dcob.o tests/*.o *.dis tests/*.dis ${NIBPROCS} \
		${SAPROCS}

prom:	niblet
	cd ..; make prom; cd niblet

nib_csu.o: nib_csu.s
	as -I. -I.. -I../../include -I$(ROOT)/usr/include \
		${NIB_AOPTS} nib_csu.s -o nib_csu.o

niblet.o: niblet.s niblet.h locks.h nsys.h nmsg.h passfail.h
	as -I. -I.. -I../../include -I$(ROOT)/usr/include \
		${NIB_AOPTS} niblet.s -o niblet.o

nib_lib.o: nib_lib.s
	as -I.. -I$(ROOT)/usr/include \
		${NIB_AOPTS} nib_lib.s -o nib_lib.o

raw_niblet: niblet.o nib_procs.o nib_lib.o
	ld -T 80100000 -D 80104000 -non_shared -coff -n niblet.o nib_lib.o -o raw_niblet
	$(TOOLROOT)/usr/bin/dis -h raw_niblet >raw_niblet.dis
	$(TOOLROOT)/usr/bin/nm -Bnx raw_niblet >raw_niblet.nm

nib_util.o: nib_util.c
	cc ${NIB_COPTS} -G 0 -c -g -I nib_util.c -o nib_util.o

scob: scob.c cob.h niblet.h
	${HOST_CC} scob.c -I -I. -I/usr/include  -L/usr/lib -fullwarn -lelf -lmld -o scob

../IP19.O/dcob.o: dcob.c cob.h niblet.h nib_procs.c
	cc -G 0 -c -g -I -I. -I$(ROOT)/usr/include ${NIB_COPTS}\
		-o ../IP19.O/dcob.o dcob.c

nib_procs.c: ${NIBPROCS} cob.h scob
	./scob ${NIBPROCS}

sa_procs.c: ${SAPROCS} cob.h scob
	./scob -o sa_procs ${SAPROCS}

../IP19.O/nib_procs.o: nib_procs.c 
	cc -non_shared -coff -G 0 -c -g -I nib_procs.c -o ../IP19.O/nib_procs.o

sa_procs.o: sa_procs.c 
	cc -non_shared -coff -G 0 -c -g -I sa_procs.c -o sa_procs.o

nextract: nextract.c
	${HOST_CC} -g nextract.c -fullwarn -I -I/usr/include -L/usr/lib -lelf -lmld -o nextract

nib_code.c: raw_niblet nextract
	./nextract raw_niblet >nib_code.c

../IP19.O/nib_code.o: nib_code.c
	cc -non_shared -coff -G 0 -c -g -I nib_code.c -o ../IP19.O/nib_code.o

../IP19.O/nib_conf.o: nib_conf.c nib_procs.h niblet.h
	cc -non_shared -coff -G 0 -c -g -I nib_conf.c -o ../IP19.O/nib_conf.o

sa_conf.o: sa_conf.c sa_procs.h niblet.h
	cc -non_shared -coff -G 0 -c -g -I sa_conf.c -o sa_conf.o

