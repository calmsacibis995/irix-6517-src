#!smake

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include	$(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=test1 abg ttos abg3 tkperf

WOFF=
#OPTIMIZER=
LCDEFS+=-DDPSTATIC=static
#LCDEFS+=-DTK_DEBUG -DTK_METER -DTK_TRC -g
LCOPTS+=-fullwarn
LIBRARY=libtkm.a

LLDLIBS=$(LIBRARY)
LOBJECTS=tkc.o tks.o tkcommon.o mbox.o
LDIRT=$(LIBRARY)

default: $(TARGETS)

include $(CMDCOMMONRULES)

$(COMMANDS):$(LIBRARY)

test1 test2 abg:$$@.o
	${CCF} $@.o -o $@ ${LDFLAGS}

$(LIBRARY):$(LIBRARY)($(LOBJECTS)) MAKELIB
