#
# Makefile for IP30 standalone mpkiller test
#
# $Revision: 1.3 $
#
# Starting address at 0xa800000020600000
#
DEPTH= ../../..
LLIBDEPTH=
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)
include $(CPUARCHDEFS)

STIDBTAG = "systest.sw.systest mach(CPUBOARD=IP30)"

LLCDEFS= -D$(CPUBOARD)
LLDFLAGS= -m -e start -N -T a800000020600000 -G 0 -64
LCOPTS= -G 0 -non_shared -64

LDV= ${LD} ${LLDFLAGS} ${VLDFLAGS} ${ENDIAN}

POSTLDOP = cp $@ $@.stripped; \
	$(STRIP) -s $@.stripped > /dev/null 2>&1 || true

CFILES= mpkiller.c
TARGETS= mpkiller

OBJS= mpk_test.o mpk_handlers.o ${DEPTH}/lib/cfg/${PRODUCT}.O/*.o


default: ${TARGETS}

include ${COMMONRULES}

${TARGETS}: $$@.o $(OBJS) ${DLIBSC} ${DLIBSK}
	${LDV} $@.o $(OBJS) ${LIBSK} ${LIBSC} -o $@ >$@.map
	size -x $@
	# rcp $@ /usr/local/boot
	/bin/nm -Bxv $@ |sort > mpk_test.map

new: genSB gen_test ${TARGETS}

genSB:
	@echo XXXX Building genSB.irix XXXX
	cd sbkiller; make

gen_test:
	@echo XXXX Generate new mpkiller test XXXX
	./gen_mpkiller

install: ${TARGETS}
	$(INSTALL) -F /stand -m 755 -idb $(STIDBTAG) -src cfuse.stripped cfuse
