#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/vm
.PATH:$(KPATH)

VM_NAME=os.a
VM_CSRCS = anon.c pcache.c scache.c vnode_pcache.c vm_icrash.c

VM_OBJS=${VM_CSRCS:.c=.o}
$(VM_NAME):$(VM_NAME)($(VM_OBJS)) MAKELIB

#if defined(CELL)
SUBDIRS=cell
#else
SUBDIRS=
#endif

VM_IDBG_NAME=vmidbg.a
VM_IDBG_CSRCS=vmidbg.c
VM_IDBG_OBJS=${VM_IDBG_CSRCS:.c=.o}
$(VM_IDBG_NAME):$(VM_IDBG_NAME)($(VM_IDBG_OBJS)) MAKELIB

ITARGETS=$(VM_IDBG_NAME)
TARGETS=$(VM_NAME) $(VM_IDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=os
CFILES=$(VM_CSRCS) $(VM_IDBG_CSRCS)

FRIEND_HEADERS= scache.h
PUBLIC_HEADERS= pcache.h vnode_pcache.h
#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${ITARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${ITARGETS}
	${KDEVINSTALL} ${ITARGETS}

vmidbg.o: vmidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/vmidbg.c

vm_icrash.o: vm_icrash.c
	$(CCF) -g3 -c $(JALR) $(KPATH)/vm_icrash.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

headers!
	${INSTALL} -idb "noship" -m 444 -F /usr/include/ksys ${FRIEND_HEADERS}
	${INSTALL} -m 444 -F /usr/include/ksys ${PUBLIC_HEADERS}
