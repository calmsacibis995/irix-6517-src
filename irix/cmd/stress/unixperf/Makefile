#!smake
#
# Makefile for unixperf
#
BASEVERSION=32bit
ALTVERSIONS=64bit n32bit r10k
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

HFILES = unixperf.h mp.h rgbdata.h random.h \
         test_cpu.h test_fileio.h test_ipc.h test_misc.h test_stdio.h test_vm.h \
	 test_cmd.h test_jmp.h test_des.h test_c4.h test_naming.h
CFILES = unixperf.c tests.c globals.c utils.c describe.c fastdes.c posix_txt.c \
         rpc_service_clnt.c rpc_service_simp.c test_rpc.c test_regex.c \
         test_cpu.c test_fileio.c test_ipc.c test_misc.c test_stdio.c test_vm.c \
         rgbdata.c random.c dhry_1.c dhry_2.c test_cmd.c test_jmp.c test_des.c \
	 test_c4.c c4_best.c c4_play.c c4_trans.c test_naming.c mp.c
LLDLIBS = -lm

#if $(RELEASE_MAJOR) < 6
REGEXP_FLAGS = -DHAS_NO_REGEX
#endif

LCFLAGS = $(CPUTYPE) $(REGEXP_FLAGS)
LDIRT = rpc_service.h rpc_service_clnt.c rpc_service_svc.c

COMMANDS = unixperf

default: $(TARGETS)

include $(CMDCOMMONRULES)

unixperf: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

.ORDER: rpc_service.h rpc_service_clnt.c rpc_service_svc.c
rpc_service.h rpc_service_clnt.c rpc_service_svc.c: rpc_service.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -P rpc_service.x

./rpc_service_svc.c: rpc_service_svc.c
./rpc_service_clnt.c: rpc_service_clnt.c
./rpc_service.h: rpc_service.h
rpc_service_simp.c: rpc_service.h rpc_service_svc.c
test_rpc.c: rpc_service.h

# install: 32bitinstall 64bitinstall n32bitinstall

32bitinstall: default
	$(INSTALL) -F /usr/stress $(COMMANDS)

64bitinstall: default
	$(INSTALL) -F /usr/stress $(IDB_TAG64) -src unixperf unixperf.64bit

n32bitinstall: default
	$(INSTALL) -F /usr/stress -src unixperf unixperf.n32bit

r10kinstall: default
	$(INSTALL) -F /usr/stress -idb "mach(CPUARCH!=R4000)" -src unixperf unixperf.n32r10k
