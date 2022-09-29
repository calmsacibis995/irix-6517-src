#$Revision: 1.4 $
DEPTH= ..
LLIBDEPTH=
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)
include $(ENDIANDEFS)

STIDBTAG = "systest.sw.systest mach(CPUBOARD=IP28)"
LLCDEFS=-D${CPUBOARD}
LLDFLAGS=-rom -non_shared ${CPUARCH_MI_LDOPTS} -T 9800000020400000

LDV= ${LD} ${LLDFLAGS} ${VLDFLAGS} ${ENDIAN} 

POSTLDOP = cp $@ $@.stripped; \
	$(STRIP) -s $@.stripped > /dev/null 2>&1 || true

CFILES= fuse.c unfuse.c fcommon.c cfuse.c 
TARGETS= fuse unfuse cfuse 

#FUNC=	fuse unfuse cfuse 

default: ${TARGETS}

include ${COMMONRULES}

${TARGETS}: $$@.o fcommon.o ${DLIBSC} ${DLIBSK}
	${LDV} $@.o fcommon.o ../lib/cfg/${PRODUCT}.O/*.o  -e start ${LIBSK} ${LIBSC} -o $@ >$@.map

	@if test "cfuse" = $@ ; \
	then\
		echo "Building cfuse";\
		rm $@.o;\
		$(CCF) -DFUNC -c cfuse.c -o $@.o ; $(POSTLDOP)\
		echo "Building cfuse.func";\
		${LDV} $@.o fcommon.o ../lib/cfg/${PRODUCT}.O/*.o  ${LIBSK} ${LIBSC} -o $@.func >$@.map;\
	fi

#${FUNC}: $$@.o  fcommon.o ${DLIBSC} ${DLIBSK}

install: ${TARGETS}
	$(INSTALL) -F /stand -m 755 -idb $(STIDBTAG) -src cfuse.stripped cfuse
