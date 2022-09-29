#!smake
# Copyright 1996, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/SN
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)
#if $(CPUBOARD) == "IP27"
SUBDIRS+=SN0
#elif $(CPUBOARD) == "IP33"
SUBDIRS+=SN1
#endif
ML_NAME=ml.a
ML_CSRCS= du.c	 groupintr.c  init.c  \
	klarcs.c klclock.c klconflib.c \
	klgraph.c klnuma.c klprobe.c  promlog.c promif.c	\
	memsupport.c mp.c stubs.c
ML_ASRCS= slave.s
#if $(CPUBOARD) == "IP27"
ML_CSRCS+=error_dump.c fprom.c error_md_force.c error_io_force.c error_ni_force.c \
	error_force.c intr.c \
	io.c iobus.c iograph.c \
	klerror.c md_perf.c io_perf.c nvram.c refcounters.c vector.c router.c
#elif $(CPUBOARD) == "IP33"
ML_CSRCS+=io.c iograph.c iobus.c
#ML_ASRCS+=
#endif


ML_OBJS=${ML_CSRCS:.c=.o} module.o ${ML_ASRCS:.s=.o}

KLIDBG_NAME=klidbg.o
KLIDBG_CSRCS=klidbg.c
KLIDBG_OBJS=${KLIDBG_CSRCS:.c=.o}

PART_NAME=partition.o
PART_CSRCS=partition.c
PART_OBJS=${PART_CSRCS:.c=.o}

XPC_NAME=xpc.o
XPC_CSRCS=xpc.c
XPC_OBJS=${XPC_CSRCS:.c=.o}

IFCL_NAME=if_cl.o
IFCL_CSRCS=if_cl.c
IFCL_OBJS=${IFCL_CSRCS:.c=.o}

$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB

#if $(CPUBOARD) == "IP33"
TARGETS=$(ML_NAME)
#else
TARGETS=$(ML_NAME) $(KLIDBG_NAME) ${PART_NAME} ${XPC_NAME} ${IFCL_NAME}
#endif
.PRECIOUS:$(TARGETS)
LINTNAME=sn
CFILES=$(ML_CSRCS) $(KLIDBG_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}
#
# This hides the symbols used to store the system serial number.
#
module.o: module.c
	$(CC) $(CFLAGS) -c $(KPATH)/module.c -o _module.o
	$(LD) $(LDFLAGS) -r _module.o -o module.o
	$(STRIP) -f module.o
	rm -f _module.o


#if $(CPUBOARD) == "IP27"
#
# 
# Since the SN0 stuff goes into the ml lib - there is no reason to install
# it here
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KDEVINSTALL} ${KLIDBG_NAME}
	${KINSTALL} -idb eoe.sw.kdebug ${KLIDBG_NAME}
	${KDEVINSTALL} ${PART_NAME}
	${KINSTALL} ${PART_NAME}
	${KDEVINSTALL} ${XPC_NAME}
	${KINSTALL} ${XPC_NAME}
	${KDEVINSTALL} ${IFCL_NAME}
	${KINSTALL} ${IFCL_NAME}
#endif

