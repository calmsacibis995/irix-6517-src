#!smake
#
PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmnscomp.c
TARGETS	= pmnscomp
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	=  $(DEBUG) $(VERSOPT)

#ifdef LLDOPTS
# This is how we get the right stuff for the make in ../buildtools
# and avoid the warning about _pmGetLicenseCap symbol preemption
#if $(IRIX) != "IRIX5_3"
LDWOFF	+= -Wl,-woff,85
#endif
#else
LLDOPTS	= $(PCP_LIBS_PATH)
#endif

LLDLIBS	= -lpcp
IDBTAG	= PCP_EOE
LDIRT	= *.pag *.dir *.log

default: $(CFILES) $(TARGETS)

include $(COMMONRULES)

exports:	install
install: $(TARGETS)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmnscomp
#if $(PCP_EOE_NOSHIP)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "PCP_EOE_NOSHIP $(STRIPBIN)" pmnscomp
#endif

pmnscomp:	pmnscomp.o
	rm -f $@
	$(CCF) pmnscomp.o $(LDWOFF) $(LDFLAGS) -o $@
	$(TAG) 0x00010D13 $@
