#!smake
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= newhelp.c
TARGETS	= newhelp

#if $(ISM_NAME) == "pcp"
CFILES += chkhelp.c all.c chkhelp_v1.c
TARGETS += chkhelp chkhelp_v1
#endif

LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG) $(VERSOPT)

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
LDIRT	= *.pag *.dir *.log

default: $(TARGETS)

include $(COMMONRULES)

exports:	install

install: $(TARGETS)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "PCP_EOE $(STRIPBIN)" newhelp
#if $(ISM_NAME) == "pcp"
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "PCP_BASE $(STRIPBIN)" chkhelp
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "PCP_BASE $(STRIPBIN)" chkhelp_v1
#endif

newhelp:	newhelp.o
	rm -f $@
	$(CCF) newhelp.o $(LDWOFF) $(LDFLAGS) -o $@
	$(TAG) 0x00010D13 $@

chkhelp:	chkhelp.o
	rm -f $@
	$(CCF) chkhelp.o $(LDFLAGS) -o $@ -lpcp_pmda
	$(TAG) 0x00010D13 $@

chkhelp_v1:	chkhelp_v1.c all.c
	rm -f $@
	$(CC) $(DEF_CSTYLE) -fullwarn -I$(PCP_SRC_DEPTH)/include1.x $(LCDEFS) chkhelp_v1.c all.c -o $@ -L$(PCP_SRC_DEPTH)/libpcp1.x/$(DEF_OBJECT_STYLE) -lpcp
	$(TAG) 0x00010D13 $@
