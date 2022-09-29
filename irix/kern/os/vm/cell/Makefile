#!smake
# Copyright 1997, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/vm/cell
.PATH:$(KPATH)

VM_NAME=os.a

VM_IDLS=exim.idl
VM_CSRCS = customs.c

VM_CSRCS+=${VM_IDLS:S/^/I_/g:.idl=_stubs.c}
KERNIDLFILES=${VM_IDLS}

VM_OBJS=${VM_CSRCS:.c=.o}
$(VM_NAME):$(VM_NAME)($(VM_OBJS)) MAKELIB

TARGETS=$(VM_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=os
CFILES=$(VM_CSRCS)

FRIEND_HEADERS= 
#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	${INSTALL} -idb "noship" -m 444 -F /usr/include/ksys ${FRIEND_HEADERS}
