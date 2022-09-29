#!smake
# Copyright 1995, 1996 Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for the numa management subsystem
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/numa
.PATH:$(KPATH)
# MKDEPFILE=$(LMKDEPFILE)
KCOPTS=-fullwarn


# 
# Basic Memory Management Control Interface 
#
MMCI_CSRCS= pmo_base.c pmo_error.c pmo_init.c aspm.c pmo_ns.c \
            pm.c pmfactory_ns.c pmo_xface.c  pm_policy_common.c afflink.c
            

#
# Basic memory migration mechanisms
#
MIGR_CSRCS= migr_engine.c migr_tests.c migr_coald.c debug_levels.c 

#
# NUMA specific mechanisms and policies
#
#if defined(KMODEOPT) && $(KMODEOPT:M-DNUMA_BASE) == "-DNUMA_BASE"
#if $(PRODDEFS:M-DSN1) != "-DSN1"
NUMA_BASE_CSRCS = numa_init.c   \
                  migr_interrupt.c mem_tick.c migr_control.c pfms.c \
                  migr_states.c migr_manager.c migr_init.c \
                  migr_bounce.c migr_queue.c numa_tune.c \
                  numa_stats.c numa_tests.c \
		  memfit.c \
	          mld.c mldset.c pmo_process.c raff.c pmo_list.c \
                  memsched.c \
	          plac_policy_default.c plac_policy_fixed.c plac_policy_thread.c\
	          plac_policy_firsttouch.c  plac_policy_roundrobin.c \
                  plac_policy_cachecolor.c \
	          fbck_policy_default.c fbck_policy_local.c \
	          migr_policy_default.c affinity.c \
	          migr_user.c migr_policy_control.c migr_policy_refcnt.c \
		  mem_profiler.c migr_refcnt.c mem_refcnt.c

NUMA_IDBG_NAME=numa_idbg.a
NUMA_IDBG_CSRCS=numa_idbg.c
NUMA_IDBG_OBJS=${NUMA_IDBG_CSRCS:.c=.o}
$(NUMA_IDBG_NAME):$(NUMA_IDBG_NAME)($(NUMA_IDBG_OBJS)) MAKELIB

#else
NUMA_BASE_CSRCS = numa_init.c   migr_control.c migr_queue.c pfms.c\
                  mem_tick.c numa_tune.c migr_policy_default.c\
                  numa_stats.c numa_tests.c \
	          mld.c mldset.c pmo_process.c raff.c pmo_list.c \
                  memsched.c memfit.c\
	          plac_policy_default.c plac_policy_fixed.c plac_policy_thread.c \
	          plac_policy_firsttouch.c  plac_policy_roundrobin.c \
                  plac_policy_cachecolor.c \
	          fbck_policy_default.c fbck_policy_lpg.c \
		  migr_policy_control.c migr_policy_refcnt.c 
#endif

#else
NUMA_BASE_CSRCS= uma_init.c numa_stubs.c pmo_stubs.c \
                 plac_policy_uma.c fbck_policy_uma.c migr_policy_uma.c \
                 repl_policy_uma.c pm_policy_stubs.c

NUMA_IDBG_NAME=
NUMA_IDBG_CSRCS=
NUMA_IDBG_OBJS=
#endif


#
# Platform dependent NUMA
#
#if defined(CPUBOARD) && ($(CPUBOARD) == "IP27")
NUMA_SN0_CSRCS= migr_traffic.c migr_unpegging.c numa_hw.c numa_traffic.c
#else
NUMA_SN0_CSRCS= 
#endif

#if defined(KMODEOPT) && $(KMODEOPT:M-DNUMA_REPLICATION) == "-DNUMA_REPLICATION"
NUMA_REPL_CSRCS=replinit.c repl_pageops.c repl_vnodeops.c \
		repl_policy.c repl_policy_default.c repl_shoot.c repldebug.c \
                repl_policy_one.c  repl_control.c
#else
NUMA_REPL_CSRCS=
#endif

NUMA_NAME=numa.a

NUMA_CSRCS= $(MMCI_CSRCS)       \
            $(MIGR_CSRCS)       \
            $(NUMA_BASE_CSRCS)  \
            $(NUMA_SN0_CSRCS)   \
            $(NUMA_REPL_CSRCS) 


NUMA_OBJS=${NUMA_CSRCS:.c=.o}
$(NUMA_NAME):$(NUMA_NAME)($(NUMA_OBJS)) MAKELIB

ITARGETS=$(NUMA_IDBG_NAME)
TARGETS=$(NUMA_NAME) $(NUMA_IDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=numa
CFILES=$(NUMA_CSRCS)


#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}

headers!
	${INSTALL} -m 444 -F /usr/include/sys numa_stats.h
	${INSTALL} -m 444 -F /usr/include/sys migr_parms.h
	${INSTALL} -m 444 -F /usr/include/sys numa_tests.h

numa_idbg.o: numa_idbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/numa_idbg.c
