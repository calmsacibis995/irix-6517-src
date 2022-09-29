#ident "tools/hwreg/Makefile: $Revision: 1.12 $"

DEPTH= ../..
SHELL=/bin/sh

include $(DEPTH)/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)

MKDEPFILE=Makedepend.$(PRODUCT)

CVERSION=-xansi $(FULLWARN)
FULLWARN=-fullwarn

WORKAROUNDS=-DHUB1_WAR

LWOFF=,1068,1185,1209,1505,1506,1692${LLWOFF}

# Machine Dependent Info
#
IP19_X=\#
IP20_X=\#
IP21_X=\#
IP22_X=\#
IP25_X=\#
IP26_X=\#
IP27_X=\#
IP27XXL_X=\#
IP28_X=\#
IP30_X=\#
IP32_X=\#
4DXX_X=\#
$(CPUBOARD)_X=
$(4DXX)_X=

${IP27_X}LCINCS=-I${DEPTH}/include -I${ROOT}/usr/include ${LLCINCS}
${IP27XXL_X}LCINCS=-I${DEPTH}/include -I${ROOT}/usr/include ${LLCINCS}

${IP27_X}LCDEFS=-32 -DIP27 -DSN0 -D_KERNEL ${WORKAROUNDS} -D_PAGESZ=4096
${IP27XXL_X}LCDEFS=-32 -DIP27 -DSN0 -D_KERNEL ${WORKAROUNDS} -D_PAGESZ=4096

${IP27_X}LCOPTS=${LLCOPTS}
${IP27XXL_X}LCOPTS=${LLCOPTS}

${IP27_X}MKDEPOPT= -MDupdate $(MKDEPFILE)
${IP27XXL_X}MKDEPOPT= -MDupdate $(MKDEPFILE)

${IP27_X}ASOPTS=
${IP27XXL_X}ASOPTS=

${IP27_X}LDOPTS=
${IP27XXL_X}LDOPTS=

${IP27_X}ASFLAGS= $(CDEFS) $(CINCS) $(PRODDEFS) $(MKDEPOPT)
${IP27XXL_X}ASFLAGS= $(CDEFS) $(CINCS) $(PRODDEFS) $(MKDEPOPT)

$(IP27_X)TARGETS= hwreg
$(IP27XXL_X)TARGETS= hwreg

default: ${TARGETS}

OPTIMIZER = -g

HWREGDIR = ${DEPTH}/lib/libkl/hwreg

CFILES = \
	main.c hwreg.c \
	hwreg_hub.c hwreg_router.c hwreg_xbow.c hwreg_c0.c hwreg_dir.c

OBJECTS = ${CFILES:.c=.o}

TARGETDIR = .

CFLAGS = ${LCINCS} ${LCDEFS} ${LCOPTS} ${OPTIMIZER} ${MKDEPOPT}

hwreg.c:
	@if [ ! -f ${HWREGDIR}/$@ ]; then \
		echo 2>&1 "Must compile libkl first."; exit 1; fi
	ln -sf ${HWREGDIR}/$@ .
hwreg_hub.c:
	ln -sf ${HWREGDIR}/$@ .
hwreg_router.c:
	ln -sf ${HWREGDIR}/$@ .
hwreg_xbow.c:
	ln -sf ${HWREGDIR}/$@ .
hwreg_c0.c:
	ln -sf ${HWREGDIR}/$@ .
hwreg_dir.c:
	ln -sf ${HWREGDIR}/$@ .

hwreg: $(TARGETDIR) $(OBJECTS)
	cd $(TARGETDIR); \
	${CC} -o $@ ${CFLAGS} ${OBJECTS} ${LDOPTS}

clobber: clean
clean:
	rm -f hwreg.c hwreg_hub.c hwreg_router.c
	rm -f hwreg_xbow.c hwreg_c0.c hwreg_dir.c
	rm -f ${OBJECTS} ${TARGETS} $(MKDEPFILE)
