# Makefile for the rag-tag of SGI standalone tests.
#
#$Revision: 1.37 $

DEPTH= ..
LLIBDEPTH=../

include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)
include $(CPUARCHDEFS)

IP12_X=\#
IP12EISA_X=\#
IP19_X=\#
IP20_X=\#
IP21_X=\#
IP22_X=\#
IP25_X=\#
$(CPUBOARD)$(MEMMAP)_X=

$(IP12_X)BASEADDR= -T 80080000
$(IP12_X)DBGBASEADDR= -T 80200000
$(IP12EISA_X)BASEADDR= -T 88080000
$(IP12EISA_X)DBGBASEADDR= -T 88200000
$(IP20_X)BASEADDR= -T 88080000
$(IP20_X)DBGBASEADDR= -T 88200000
$(IP22_X)BASEADDR= -T 88080000
$(IP22_X)DBGBASEADDR= -T 88200000
$(IP19_X)BASEADDR= -T 80080000
$(IP19_X)DBGBASEADDR= -T 80200000
$(IP21_X)BASEADDR= -T 80080000
$(IP21_X)DBGBASEADDR= -T 80200000
$(IP25_X)BASEADDR= -T 80080000
$(IP25_X)DBGBASEADDR= -T 80200000

# to build tests NOT relocatable: "make RELFLAGS="
LLDFLAGS= $(CPUARCH_MI_LDOPTS)
LLCDEFS= -D$(CPUBOARD)
RELFLAGS= -r -d
LDV= ${LD} ${LLDFLAGS} ${VLDFLAGS} ${ENDIAN}

SCTARGETS= wait wait2 nametest opendev argv findtape getcmp tp color \
	test dbgtest ld1 ld2 mouse writedev exit readdev
SKTARGETS= badaddr
OTHERTARGETS= utlbexc nametest
OTHERTARGETS2= dbgtest2
TARGETS= ${SCTARGETS} ${SKTARGETS} ${OTHERTARGETS} ${OTHERTARGETS2}

CFILES=$(SCTARGETS:=.c) $(SKTARGETS:=.c) $(OTHERTARGETS:=.c)

TARGETDIR= $(PRODUCT).O
VPATH= $(TARGETDIR)

INCLUDES= -I$(DEPTH)/include

LDIRT=$(OBJECTS)
COMMONPREF= ARCS

default: ${TARGETS} ${SCTARGETS} ${SKTARGETS}

include ${COMMONRULES}

${SCTARGETS}: $$@.o ${DLIBSC}
	cd $(TARGETDIR); \
	${LDV} -e startsc ${RELFLAGS} $@.o ${LIBSC} -o $@
	
${SKTARGETS}: $$@.o ${DLIBSC} ${DLIBSK} ${DEFCFG}
	cd $(TARGETDIR); \
	${LDV} -e start ${BASEADDR} $@.o ../${DEFCFG} ${LIBSK} ${LIBSC} -o $@

utlbexc: $$@.o myfault.o ${DLIBSC}
	cd $(TARGETDIR); \
	${LDV} -e startsc ${RELFLAGS} $@.o myfault.o ${LIBSC} -o $@

utlb: $$@.o utlbasm.o ${DLIBSC} ${DLIBSK} ${DEFCFG}
	cd $(TARGETDIR); \
	${LDV} -e start ${BASEADDR} $@.o utlbasm.o ../${DEFCFG} ${LIBSK} ${LIBSC} -o $@

dbgtest2: dbgtest.o ${DLIBSC}
	cd $(TARGETDIR); \
	${LDV} -e startsc ${RELFLAGS} dbgtest.o ${DEPTH}/../lib/libsc/$(ENDIANND)$(CPUARCH_PFIX).O/idbg.o ${LIBSC} -o $@
	$(TOOLROOT)/usr/sbin/setsym $(TARGETDIR)/$@

install: default
	cd $(TARGETDIR); \
	$(INSTALL) -F /usr/local/boot ${TARGETS}

.c.o:
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi
	$(CCF) $< -c -o $(TARGETDIR)/$*.o

.s.o:
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi
	$(ASF) $< -c -o $(TARGETDIR)/$*.o

clobber: clean ${COMMONPREF}clobber
	for d in $(EVERYPRODUCT); do rm -rf $$d.O; done

clean:	${COMMONPREF}clean
	@if test -d ${TARGETDIR}; then cd ${TARGETDIR}; rm -f ${LDIRT}; fi

depend incdepend: ${COMMONPREF}$$@
	@echo ${CFILES}
