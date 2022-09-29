#!smake
#
# Makefile for cell which contains generic cell/distributed processing objects
#
#ident	"$Revision: 1.32 $"

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/cell
.PATH:$(KPATH)

KCINCS=-I${BOOTAREA}

SUBDIRS=wp cms

CELL_NAME=cell.a
CELL_CSRCS=bla.c cell.c mesg.c mesg_xpc.c tkc.c tks.c tkcommon.c object.c \
	relocation.c cell_hb.c creds.c cell_set.c recovery.c kernstats.c
#if CELL && !UNIKERNEL
CELL_CSRCS+=ucopy.c utility.c
#endif

CELL_IDLS=kore.idl cell_hb.idl creds.idl
#if CELL && !UNIKERNEL
CELL_IDLS+=ucopy.idl utility.idl
#endif
#if defined(KDEBUG)
CELL_IDLS+=cell_test.idl 
#endif

CELL_CSRCS+=${CELL_IDLS:S/^/I_/g:.idl=_stubs.c}
KERNIDLFILES=${CELL_IDLS}
CELL_OBJS=${CELL_CSRCS:.c=.o}

$(CELL_NAME):$(CELL_NAME)($(CELL_OBJS)) MAKELIB

CELL_IDBG_NAME=dp_idbg.a
CELL_IDBG_CSRCS=dp_idbg.c
CELL_IDBG_OBJS=${CELL_IDBG_CSRCS:.c=.o}
$(CELL_IDBG_NAME):$(CELL_IDBG_NAME)($(CELL_IDBG_OBJS)) MAKELIB

TARGETS=$(CELL_NAME) $(CELL_IDBG_NAME)
.PRECIOUS:$(TARGETS)

# lint
LINTNAME=cell
CFILES=$(CELL_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

# This default rule is always invoked in BOOTAREA
$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}

dp_idbg.o: dp_idbg.c
	$(CCF) -c $(JALR) -G 0 $*.c
