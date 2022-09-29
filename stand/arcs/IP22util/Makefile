#$Revision: 1.5 $
DEPTH= ..
LLIBDEPTH=
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)

STIDBTAG = "systest.sw.systest mach(CPUBOARD=IP22)"

LLDFLAGS= -m -e start -N -T 88080000 -G 0
LCOPTS= -G 0 -non_shared

LDV= ${LD} ${LLDFLAGS} ${VLDFLAGS} ${ENDIAN} 

POSTLDOP = cp $@ $@.stripped; \
	$(STRIP) -s $@.stripped > /dev/null 2>&1 || true

CFILES= fuse.c unfuse.c fcommon.c cfuse.c cfuse_ibm.c
TARGETS= fuse unfuse cfuse cfuse_ibm

#FUNC=	fuse unfuse cfuse cfuse_ibm

default: ${TARGETS}

include ${COMMONRULES}

${TARGETS}: $$@.o fcommon.o ${DLIBSC} ${DLIBSK}
	${LDV} $@.o fcommon.o ../lib/cfg/${PRODUCT}.O/*.o  ${LIBSK} ${LIBSC} -o $@ >$@.map

	@if test "cfuse" = $@ ; \
	then\
		echo "Building cfuse";\
		rm $@.o;\
		$(CCF) -DFUNC -c cfuse.c -o $@.o ; $(POSTLDOP)\
		echo "Building cfuse.func";\
		${LDV} $@.o fcommon.o ../lib/cfg/${PRODUCT}.O/*.o  ${LIBSK} ${LIBSC} -o $@.func >$@.map;\
		$(ELF2COFF) $@ $@.coff ; \
		mv $@ $@.elf; \
		mv $@.coff $@; \
	fi

#${FUNC}: $$@.o  fcommon.o ${DLIBSC} ${DLIBSK}

install: ${TARGETS}
	$(INSTALL) -F /stand -m 755 -idb $(STIDBTAG) -src cfuse.stripped cfuse
