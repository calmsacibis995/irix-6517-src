#
#ident "$Revision: 1.12 $"
#
# Makefile for logical volume utilities
#
include ${ROOT}/usr/include/make/commondefs

LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
CFILES= lvtool.c lvinit.c lvck.c mklv.c labelsub.c lvtabsub.c pathnames.c \		lvdev.c lvinfo.c

UTILOBS= labelsub.o lvtabsub.o pathnames.o lvdev.o 

INITOBS= lvinit.o ${UTILOBS}

LVCKOBS= lvck.o ${UTILOBS}

MKLVOBS= mklv.o ${UTILOBS}

# don't build or install lvtool, it's just a test routine

TARGETS= lvinit lvck mklv lvinfo 

default: ${TARGETS}

include ${COMMONRULES}

install: lvinit mklv lvck lvinfo
	${INSTALL} -F /sbin lvinit lvck mklv 
	${INSTALL} -F /etc -lns ../sbin/lvinit lvinit
	${INSTALL} -F /etc -lns ../sbin/lvck lvck
	${INSTALL} -F /etc -lns ../sbin/mklv mklv
	${INSTALL} -F /sbin -u root -m 04755 lvinfo
	${INSTALL} -F /etc -lns ../sbin/lvinfo lvinfo

lvinit:	${INITOBS}
	${CCF} ${INITOBS} -o $@ $(LDFLAGS)

lvck:	${LVCKOBS}
	${CCF} ${LVCKOBS} -o $@ $(LDFLAGS)

mklv:	${MKLVOBS}
	${CCF} ${MKLVOBS} -o $@ $(LDFLAGS)

lvinfo: lvinfo.o pathnames.o
	${CCF} lvinfo.o pathnames.o -o $@ $(LDFLAGS)

